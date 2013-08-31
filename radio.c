/*
 * Clone Utility for Baofeng radios.
 *
 * Copyright (C) 2013 Serge Vakulenko, KK6ABQ
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. The name of the author may not be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef MINGW32
#   include <windows.h>
#else
#   include <termios.h>
#endif
#include <time.h>
#include <sys/stat.h>
#include "radio.h"
#include "util.h"

int radio_port;                         // File descriptor of programming serial port
unsigned char radio_ident [8];          // Radio: identifier
unsigned char radio_mem [0x2000];       // Radio: memory contents
int radio_progress;                     // Read/write progress counter

static radio_device_t *device;          // Device-dependent interface
static unsigned char image_ident [8];   // Image file: identifier
#ifdef MINGW32
DCB saved_mode;                         // Mode of serial port, Windows
#else
static struct termios oldtio, newtio;   // Mode of serial port, Unix
#endif

//
// Open the serial port.
//
static int open_port (char *portname)
{
#ifdef MINGW32
    HANDLE fd;
    DCB new_mode;
    COMMTIMEOUTS timo;

    /* Open port */
    fd = CreateFile (portname, GENERIC_READ | GENERIC_WRITE,
        0, 0, OPEN_EXISTING, 0, 0);
    if (fd == INVALID_HANDLE_VALUE) {
        fprintf (stderr, "%s: Cannot open\n", portname);
        exit (-1);
    }

    /* Set serial attributes */
    memset (&saved_mode, 0, sizeof(saved_mode));
    if (! GetCommState (fd, &saved_mode)) {
        fprintf (stderr, "%s: Cannot get state\n", portname);
        exit (-1);
    }

    new_mode = saved_mode;

    new_mode.BaudRate = CBR_9600;
    new_mode.ByteSize = 8;
    new_mode.StopBits = ONESTOPBIT;
    new_mode.Parity = 0;
    new_mode.fParity = FALSE;
    new_mode.fOutX = FALSE;
    new_mode.fInX = FALSE;
    new_mode.fOutxCtsFlow = FALSE;
    new_mode.fOutxDsrFlow = FALSE;
    new_mode.fRtsControl = RTS_CONTROL_DISABLE;
    new_mode.fNull = FALSE;
    new_mode.fAbortOnError = FALSE;
    new_mode.fBinary = TRUE;
    if (! SetCommState (fd, &new_mode)) {
        fprintf (stderr, "%s: Cannot set state\n", portname);
        exit (-1);
    }

    timo.ReadIntervalTimeout = 0;
    timo.ReadTotalTimeoutMultiplier = 0;
    timo.ReadTotalTimeoutConstant = 200;
    timo.WriteTotalTimeoutMultiplier = 1;
    timo.WriteTotalTimeoutConstant = 1000;
    SetCommTimeouts (fd, &timo);

    // Flush received data pending on the port.
    PurgeComm ((HANDLE) radio_port, PURGE_RXCLEAR);
    return (int) fd;
#else
    int fd;

    // Use non-block flag to ignore carrier (DCD).
    fd = open (portname, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        perror (portname);
        exit (-1);
    }

    // Get terminal modes.
    tcgetattr (fd, &oldtio);
    newtio = oldtio;

    newtio.c_cflag &= ~CSIZE;
    newtio.c_cflag |= CS8;              // 8 data bits
    newtio.c_cflag |= CLOCAL | CREAD;   // enable receiver, set local mode
    newtio.c_cflag &= ~PARENB;          // no parity
    newtio.c_cflag &= ~CSTOPB;          // 1 stop bit
    newtio.c_cflag &= ~CRTSCTS;         // no h/w handshake
    newtio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  // raw input
    newtio.c_oflag &= ~OPOST;           // raw output
    newtio.c_iflag &= ~IXON;            // software flow control disabled
    newtio.c_iflag &= ~ICRNL;           // do not translate CR to NL

    cfsetispeed(&newtio, B9600);        // Set baud rate.
    cfsetospeed(&newtio, B9600);

    // Set terminal modes.
    tcsetattr (fd, TCSANOW, &newtio);

    // Clear the non-block flag.
    int flags = fcntl (fd, F_GETFL, 0);
    if (flags < 0) {
        perror("F_GETFL");
        exit (-1);
    }
    flags &= ~O_NONBLOCK;
    if (fcntl (fd, F_SETFL, flags) < 0) {
        perror("F_SETFL");
        exit (-1);
    }

    // Flush received data pending on the port.
    tcflush (fd, TCIFLUSH);
    return fd;
#endif
}

//
// Close the serial port.
//
void radio_disconnect()
{
    fprintf (stderr, "Close device.\n");

    // Restore the port mode.
#ifdef MINGW32
    SetCommState ((HANDLE) radio_port, &saved_mode);
    CloseHandle ((HANDLE) radio_port);
#else
    tcsetattr (radio_port, TCSANOW, &oldtio);
    close (radio_port);
    radio_port = -1;
#endif

    // Radio needs a timeout to reset to a normal state.
    mdelay (2000);
}

//
// Print a generic information about the device.
//
void radio_print_version (FILE *out)
{
    fprintf (out, "Radio: %s\n", device->name);
    device->print_version (out);
}

//
// Try to identify the device with a given magic command.
// Return 0 when failed.
//
static int try_magic (const unsigned char *magic)
{
    unsigned char reply;
    int magic_len = strlen ((char*) magic);

    // Send magic.
    if (verbose) {
        printf ("# Sending magic: ");
        print_hex (magic, magic_len);
        printf ("\n");
    }
#ifdef MINGW32
    PurgeComm ((HANDLE) radio_port, PURGE_RXCLEAR);
#else
    tcflush (radio_port, TCIFLUSH);
#endif
    serial_write (radio_port, magic, magic_len);

    // Check response.
    if (serial_read (radio_port, &reply, 1) != 1) {
        if (verbose)
            fprintf (stderr, "Radio did not respond.\n");
        return 0;
    }
    if (reply != 0x06) {
        fprintf (stderr, "Bad response: %02x\n", reply);
        return 0;
    }

    // Query for identifier..
    serial_write (radio_port, "\x02", 1);
    if (serial_read (radio_port, radio_ident, 8) != 8) {
        fprintf (stderr, "Empty identifier.\n");
        return 0;
    }
    if (verbose) {
        printf ("# Identifier: ");
        print_hex (radio_ident, 8);
        printf ("\n");
    }

    // Enter clone mode.
    serial_write (radio_port, "\x06", 1);
    if (serial_read (radio_port, &reply, 1) != 1) {
        fprintf (stderr, "Radio refused to clone.\n");
        return 0;
    }
    if (reply != 0x06) {
        fprintf (stderr, "Radio refused to clone: %02x\n", reply);
        return 0;
    }
    return 1;
}

//
// Connect to the radio and identify the type of device.
//
void radio_connect (char *port_name)
{
    static const unsigned char UV5R_MODEL_AGED[] = "\x50\xBB\xFF\x01\x25\x98\x4D";
    static const unsigned char UV5R_MODEL_291[] = "\x50\xBB\xFF\x20\x12\x07\x25";
    static const unsigned char UVB5_MODEL[] = "PROGRAM";
    int retry;

    fprintf (stderr, "Connect to %s.\n", port_name);
    radio_port = open_port (port_name);
    for (retry=0; ; retry++) {
        if (retry >= 10) {
            fprintf (stderr, "Device not detected.\n");
            exit (-1);
        }
        if (try_magic (UVB5_MODEL)) {
            if (strncmp ((char*)radio_ident, "HKT511", 6) == 0) {
                device = &radio_uvb5;   // Baofeng UV-B5, UV-B6
                break;
            }
            if (strncmp ((char*)radio_ident, "P3107", 5) == 0) {
                device = &radio_bf888s; // Baofeng BF-888S
                break;
            }
            printf ("Unrecognized identifier: ");
            print_hex (radio_ident, 8);
            printf ("\n");
        }
        mdelay (500);
        if (try_magic (UV5R_MODEL_291)) {
            device = &radio_uv5r;       // Baofeng UV-5R, UV-5RA
            break;
        }
        mdelay (500);
        if (try_magic (UV5R_MODEL_AGED)) {
            device = &radio_uv5r_aged;  // Baofeng UV-5R with old firmware
            break;
        }
        mdelay (500);
    }
    printf ("Detected %s.\n", device->name);
}

//
// Read firmware image from the device.
//
void radio_download()
{
    radio_progress = 0;
    if (! verbose)
        fprintf (stderr, "Read device: ");

    device->download();

    if (! verbose)
        fprintf (stderr, " done.\n");

    // Copy device identifier to image identifier,
    // to allow writing it back to device.
    memcpy (image_ident, radio_ident, sizeof(radio_ident));
}

//
// Write firmware image to the device.
//
void radio_upload()
{
    // Check for compatibility.
    if (memcmp (image_ident, radio_ident, sizeof(radio_ident)) != 0) {
        fprintf (stderr, "Incompatible image - cannot upload.\n");
        exit(-1);
    }
    radio_progress = 0;
    if (! verbose)
        fprintf (stderr, "Write device: ");

    device->upload();

    if (! verbose)
        fprintf (stderr, " done.\n");
}

//
// Read firmware image from the binary file.
//
void radio_read_image (char *filename)
{
    FILE *img;
    struct stat st;

    fprintf (stderr, "Read image from file '%s'.\n", filename);

    // Guess device type by file size.
    if (stat (filename, &st) < 0) {
        perror (filename);
        exit (-1);
    }
    switch (st.st_size) {
    case 6472:
        device = &radio_uv5r;
        break;
    case 6152:
        device = &radio_uv5r_aged;
        break;
    case 4144:
        device = &radio_uvb5;
        break;
    case 992:
        device = &radio_bf888s;
        break;
    default:
        fprintf (stderr, "%s: Unrecognized file size %u bytes.\n",
            filename, (int) st.st_size);
        exit (-1);
    }

    img = fopen (filename, "rb");
    if (! img) {
        perror (filename);
        exit (-1);
    }
    device->read_image (img, image_ident);
    fclose (img);
}

//
// Save firmware image to the binary file.
//
void radio_save_image (char *filename)
{
    FILE *img;

    fprintf (stderr, "Write image to file '%s'.\n", filename);
    img = fopen (filename, "w");
    if (! img) {
        perror (filename);
        exit (-1);
    }
    device->save_image (img);
    fclose (img);
}

//
// Read the configuration from text file, and modify the firmware.
//
void radio_parse_config (char *filename)
{
    FILE *conf;
    char line [256], *p, *v;
    int table_id = 0, table_dirty = 0;

    fprintf (stderr, "Read configuration from file '%s'.\n", filename);
    conf = fopen (filename, "r");
    if (! conf) {
        perror (filename);
        exit (-1);
    }

    while (fgets (line, sizeof(line), conf)) {
        // Strip trailing spaces and newline.
        line[sizeof(line)-1] = 0;
        v = line + strlen(line) - 1;
        while (v >= line && (*v=='\n' || *v=='\r' || *v==' ' || *v=='\t'))
            *v-- = 0;

        // Ignore comments and empty lines.
        p = line;
        if (*p == '#' || *p == 0)
            continue;

        if (*p != ' ') {
            // Table finished.
            table_id = 0;

            // Find the value.
            v = strchr (p, ':');
            if (! v) {
                // Table header: get table type.
                table_id = device->parse_header (p);
                if (! table_id) {
badline:            fprintf (stderr, "Invalid line: '%s'\n", line);
                    exit(-1);
                }
                table_dirty = 0;
                continue;
            }

            // Parameter.
            *v++ = 0;

            // Skip spaces.
            while (*v == ' ' || *v == '\t')
                v++;

            device->parse_parameter (p, v);

        } else {
            // Table row or comment.
            // Skip spaces.
            // Ignore comments and empty lines.
            while (*p == ' ' || *p == '\t')
                p++;
            if (*p == '#' || *p == 0)
                continue;
            if (! table_id)
                goto badline;

            if (! device->parse_row (table_id, ! table_dirty, p))
                goto badline;
            table_dirty = 1;
        }
    }
    fclose (conf);
}

//
// Print full information about the device configuration.
//
void radio_print_config (FILE *out, int verbose)
{
    if (verbose) {
        char buf [40];
        time_t t;
        struct tm *tmp;

        t = time (NULL);
        tmp = localtime (&t);
        if (! tmp || ! strftime (buf, sizeof(buf), "%Y/%m/%d ", tmp))
            buf[0] = 0;
        fprintf (out, "#\n");
        fprintf (out, "# This configuration was generated %sby BaoClone Utility,\n", buf);
        fprintf (out, "# Version %s, %s\n", version, copyright);
        fprintf (out, "#\n");
    }
    device->print_config (out, verbose);
}
