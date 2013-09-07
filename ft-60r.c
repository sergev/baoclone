/*
 * Interface to Yaesu FT-60R.
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
#include <stdint.h>
#include "radio.h"
#include "util.h"

#define NCHAN 1000

//
// Print a generic information about the device.
//
static void ft60r_print_version (FILE *out)
{
    // Nothing to print.
}

//
// Read block of data, up to 8 bytes.
// Halt the program on any error.
//
static void read_block (int fd, int start, unsigned char *data, int nbytes)
{
    unsigned char cmd[4], reply[4];
    int addr, len;

    // Send command.
    cmd[0] = 'R';
    cmd[1] = start >> 8;
    cmd[2] = start;
    cmd[3] = nbytes;
    serial_write (fd, cmd, 4);

    // Read reply.
    if (serial_read (fd, reply, 4) != 4) {
        fprintf (stderr, "Radio refused to send block 0x%04x.\n", start);
        exit(-1);
    }
    addr = reply[1] << 8 | reply[2];
    if (reply[0] != 'W' || addr != start || reply[3] != nbytes) {
        fprintf (stderr, "Bad reply for block 0x%04x of %d bytes: %02x-%02x-%02x-%02x\n",
            start, nbytes, reply[0], reply[1], reply[2], reply[3]);
        exit(-1);
    }

    // Read data.
    len = serial_read (fd, data, 8);
    if (len != nbytes) {
        fprintf (stderr, "Reading block 0x%04x: got only %d bytes.\n", start, len);
        exit(-1);
    }

    // Get acknowledge.
    serial_write (fd, "\x06", 1);
    if (serial_read (fd, reply, 1) != 1) {
        fprintf (stderr, "No acknowledge after block 0x%04x.\n", start);
        exit(-1);
    }
    if (reply[0] != 0x06) {
        fprintf (stderr, "Bad acknowledge after block 0x%04x: %02x\n", start, reply[0]);
        exit(-1);
    }
    if (verbose) {
        printf ("# Read 0x%04x: ", start);
        print_hex (data, nbytes);
        printf ("\n");
    } else {
        ++radio_progress;
        if (radio_progress % 4 == 0) {
            fprintf (stderr, "#");
            fflush (stderr);
        }
    }
}

//
// Write block of data, up to 8 bytes.
// Halt the program on any error.
//
static void write_block (int fd, int start, const unsigned char *data, int nbytes)
{
    unsigned char cmd[4], reply;

    // Send command.
    cmd[0] = 'W';
    cmd[1] = start >> 8;
    cmd[2] = start;
    cmd[3] = nbytes;
    serial_write (fd, cmd, 4);
    serial_write (fd, data, nbytes);

    // Get acknowledge.
    if (serial_read (fd, &reply, 1) != 1) {
        fprintf (stderr, "No acknowledge after block 0x%04x.\n", start);
        exit(-1);
    }
    if (reply != 0x06) {
        fprintf (stderr, "Bad acknowledge after block 0x%04x: %02x\n", start, reply);
        exit(-1);
    }

    if (verbose) {
        printf ("# Write 0x%04x: ", start);
        print_hex (data, nbytes);
        printf ("\n");
    } else {
        ++radio_progress;
        if (radio_progress % 4 == 0) {
            fprintf (stderr, "#");
            fflush (stderr);
        }
    }
}

//
// Read memory image from the device.
//
static void ft60r_download()
{
    int addr;

    memset (radio_mem, 0xff, 0x400);
    for (addr=0x10; addr<0x110; addr+=8)
        read_block (radio_port, addr, &radio_mem[addr], 8);
    for (addr=0x2b0; addr<0x2c0; addr+=8)
        read_block (radio_port, addr, &radio_mem[addr], 8);
    for (addr=0x3c0; addr<0x3e0; addr+=8)
        read_block (radio_port, addr, &radio_mem[addr], 8);
}

//
// Write memory image to the device.
//
static void ft60r_upload()
{
    int addr;

    for (addr=0x10; addr<0x110; addr+=8)
        write_block (radio_port, addr, &radio_mem[addr], 8);
    for (addr=0x2b0; addr<0x2c0; addr+=8)
        write_block (radio_port, addr, &radio_mem[addr], 8);
    for (addr=0x3c0; addr<0x3e0; addr+=8)
        write_block (radio_port, addr, &radio_mem[addr], 8);
}

#if 0
static void decode_squelch (uint16_t bcd, int *ctcs, int *dcs)
{
    if (bcd == 0 || bcd == 0xffff) {
        // Squelch disabled.
        return;
    }
    int index = ((bcd >> 12) & 15) * 1000 +
                ((bcd >> 8)  & 15) * 100 +
                ((bcd >> 4)  & 15) * 10 +
                (bcd         & 15);

    if (index < 8000) {
        // CTCSS value is Hz multiplied by 10.
        *ctcs = index;
        *dcs = 0;
        return;
    }
    // DCS mode.
    if (index < 12000)
        *dcs = index - 8000;
    else
        *dcs = - (index - 12000);
    *ctcs = 0;
}

//
// Convert squelch string to tone value in BCD format.
// Four possible formats:
// nnn.n - CTCSS frequency
// DnnnN - DCS normal
// DnnnI - DCS inverted
// '-'   - Disabled
//
static int encode_squelch (char *str)
{
    unsigned val;

    if (*str == 'D' || *str == 'd') {
        // DCS tone
        char *e;
        val = strtol (++str, &e, 10);
        if (val < 1 || val >= 999)
            return 0;

        if (*e == 'N' || *e == 'n') {
            val += 8000;
        } else if (*e == 'I' || *e == 'i') {
            val += 12000;
        } else {
            return 0;
        }
    } else if (*str >= '0' && *str <= '9') {
        // CTCSS tone
        float hz;
        if (sscanf (str, "%f", &hz) != 1)
            return 0;

        // Round to integer.
        val = hz * 10.0 + 0.5;
    } else {
        // Disabled
        return 0;
    }

    int bcd = ((val / 1000) % 16) << 12 |
              ((val / 100)  % 10) << 8 |
              ((val / 10)   % 10) << 4 |
              (val          % 10);
    return bcd;
}
#endif

typedef struct {
    uint8_t     duplex    : 4,
                isam      : 1,
                isnarrow  : 1,
                _u1       : 1,
                used      : 1;
    uint8_t     rxfreq [3];
    uint8_t     tmode     : 3,
                step      : 3,
                _u2       : 2;
    uint8_t     txfreq [3];
    uint8_t     tone      : 6,
                power     : 2;
    uint8_t     dtcs      : 7,
                _u3       : 1;
    uint8_t     _u4 [2];
    uint8_t     offset;
    uint8_t     _u5 [3];
} memory_channel_t;

//
// Convert 32-bit value from binary coded decimal
// to integer format (8 digits).
//
int bcd6_to_int (uint8_t *bcd)
{
    return ((bcd[0] >> 4) & 15) * 100000 +
            (bcd[0]       & 15) * 10000 +
           ((bcd[1] >> 4) & 15) * 1000 +
            (bcd[1]       & 15) * 100 +
           ((bcd[2] >> 4) & 15) * 10 +
            (bcd[2]       & 15);
}

static void decode_channel (int i, int *rx_hz, int *tx_hz,
    int *rx_ctcs, int *tx_ctcs, int *rx_dcs, int *tx_dcs,
    int *lowpower, int *wide, int *scan, int *isam, int *duplex)
{
    memory_channel_t *ch = i + (memory_channel_t*) &radio_mem[0x0248];

    *rx_hz = *tx_hz = *rx_ctcs = *tx_ctcs = *rx_dcs = *tx_dcs = 0;
    if (! ch->used)
        return;

    // Decode channel frequencies.
    *rx_hz = bcd6_to_int (ch->rxfreq) * 10000;
    *tx_hz = bcd6_to_int (ch->txfreq) * 10000;

    // Decode squelch modes.
    //decode_squelch (ch->rxtone, rx_ctcs, rx_dcs);
    //decode_squelch (ch->txtone, tx_ctcs, tx_dcs);

    // Other parameters.
    *lowpower = ! ch->power;
    *wide = ! ch->isnarrow;
    *scan = 0; // TODO
    *isam = ch->isam;
    *duplex = ch->duplex;
}

#if 0
static void setup_channel (int i, double rx_mhz, double tx_mhz,
    int rq, int tq, int highpower, int wide, int scan, int isam, int duplex)
{
    memory_channel_t *ch = i + (memory_channel_t*) &radio_mem[0x10];

    ch->rxfreq = int_to_bcd ((int) (rx_mhz * 100000.0));
    ch->txfreq = int_to_bcd ((int) (tx_mhz * 100000.0));
    ch->rxtone = rq;
    ch->txtone = tq;
    ch->highpower = highpower;
    ch->narrow = ! wide;
    ch->noscan = ! scan;
    ch->noisam = ! isam;
    ch->noscr = ! duplex;
}
#endif

static void print_offset (FILE *out, int delta)
{
    if (delta == 0) {
        fprintf (out, " 0      ");
    } else {
        if (delta > 0) {
            fprintf (out, "+");;
        } else {
            fprintf (out, "-");;
            delta = - delta;
        }
        if (delta % 1000000 == 0)
            fprintf (out, "%-7u", delta / 1000000);
        else
            fprintf (out, "%-7.3f", delta / 1000000.0);
    }
}

static void print_squelch (FILE *out, int ctcs, int dcs)
{
    if      (ctcs)    fprintf (out, "%5.1f", ctcs / 10.0);
    else if (dcs > 0) fprintf (out, "D%03dN", dcs);
    else if (dcs < 0) fprintf (out, "D%03dI", -dcs);
    else              fprintf (out, "   - ");
}

//
// Print full information about the device configuration.
//
static void ft60r_print_config (FILE *out, int verbose)
{
    int i;

    // Print memory channels.
    fprintf (out, "\n");
    if (verbose) {
        fprintf (out, "# Table of preprogrammed channels.\n");
        //fprintf (out, "# 1) Channel number: 1-%d\n", NCHAN);
        //fprintf (out, "# 2) Receive frequency in MHz\n");
        //fprintf (out, "# 3) Offset of transmit frequency in MHz\n");
        //fprintf (out, "# 4) Squelch tone for receive, or '-' to disable\n");
        //fprintf (out, "# 5) Squelch tone for transmit, or '-' to disable\n");
        //fprintf (out, "# 6) Transmit power: Low, High\n");
        //fprintf (out, "# 7) Modulation width: Wide, Narrow\n");
        //fprintf (out, "# 8) Add this channel to scan list\n");
        //fprintf (out, "# 9) Busy channel lockout\n");
        //fprintf (out, "# 10) Enable scrambler\n");
        //fprintf (out, "#\n");
    }
    fprintf (out, "Channel Receive  TxOffset R-Squel T-Squel Power FM     Scan AM  Duplex\n");
    for (i=0; i<NCHAN; i++) {
        int rx_hz, tx_hz, rx_ctcs, tx_ctcs, rx_dcs, tx_dcs;
        int lowpower, wide, scan, isam, duplex;

        decode_channel (i, &rx_hz, &tx_hz, &rx_ctcs, &tx_ctcs,
            &rx_dcs, &tx_dcs, &lowpower, &wide, &scan, &isam, &duplex);
        if (rx_hz == 0) {
            // Channel is disabled
            continue;
        }

        fprintf (out, "%5d   %8.4f ", i+1, rx_hz / 1000000.0);
        print_offset (out, tx_hz - rx_hz);
        fprintf (out, " ");
        print_squelch (out, rx_ctcs, rx_dcs);
        fprintf (out, "   ");
        print_squelch (out, tx_ctcs, tx_dcs);

        fprintf (out, "   %-4s  %-6s %-4s %-3s %s\n", lowpower ? "Low" : "High",
            wide ? "Wide" : "Narrow", scan ? "+" : "-",
            isam ? "+" : "-", duplex ? "+" : "-");
    }
    //if (verbose)
    //    print_squelch_tones (out);
}

//
// Read memory image from the binary file.
//
static void ft60r_read_image (FILE *img, unsigned char *ident)
{
    if (fread (&radio_mem[0], 1, 0x6fc8, img) != 0x6fc8) {
        fprintf (stderr, "Error reading image data.\n");
        exit (-1);
    }
    memcpy (ident, radio_mem, 8);
}

//
// Save memory image to the binary file.
//
static void ft60r_save_image (FILE *img)
{
    fwrite (&radio_mem[0], 1, 0x6fc8, img);
}

static void ft60r_parse_parameter (char *param, char *value)
{
    if (strcasecmp ("Radio", param) == 0) {
        if (strcasecmp ("Yaesu FT-60R", value) != 0) {
            fprintf (stderr, "Bad value for %s: %s\n", param, value);
            exit(-1);
        }
        return;
    }
    fprintf (stderr, "Unknown parameter: %s = %s\n", param, value);
    exit(-1);
}

#if 0
//
// Check that the radio does support this frequency.
//
static int is_valid_frequency (int mhz)
{
    if (mhz >= 108 && mhz <= 520)
        return 1;
    if (mhz >= 700 && mhz <= 999)
        return 1;
    return 0;
}
#endif

//
// Parse table header.
// Return table id, or 0 in case of error.
//
static int ft60r_parse_header (char *line)
{
    if (strncasecmp (line, "Channel", 7) == 0)
        return 'C';

    return 0;
}

//
// Parse one line of table data.
// Start_flag is 1 for the first table row.
// Return 0 on failure.
//
static int ft60r_parse_row (int table_id, int first_row, char *line)
{
    char num_str[256], rxfreq_str[256], offset_str[256], rq_str[256];
    char tq_str[256], power_str[256], wide_str[256], scan_str[256];
    char isam_str[256], duplex_str[256];
    int num; // rq, tq, highpower, wide, scan, isam, duplex;
    //float rx_mhz, txoff_mhz;

    if (sscanf (line, "%s %s %s %s %s %s %s %s %s %s",
        num_str, rxfreq_str, offset_str, rq_str, tq_str, power_str,
        wide_str, scan_str, isam_str, duplex_str) != 10)
        return 0;

    num = atoi (num_str);
    if (num < 1 || num > NCHAN) {
        fprintf (stderr, "Bad channel number.\n");
        return 0;
    }
#if 0
    if (sscanf (rxfreq_str, "%f", &rx_mhz) != 1 ||
        ! is_valid_frequency (rx_mhz))
    {
        fprintf (stderr, "Bad receive frequency.\n");
        return 0;
    }
    if (sscanf (offset_str, "%f", &txoff_mhz) != 1 ||
        ! is_valid_frequency (rx_mhz + txoff_mhz))
    {
        fprintf (stderr, "Bad transmit offset.\n");
        return 0;
    }
    rq = encode_squelch (rq_str);
    tq = encode_squelch (tq_str);

    if (strcasecmp ("High", power_str) == 0) {
        highpower = 1;
    } else if (strcasecmp ("Low", power_str) == 0) {
        highpower = 0;
    } else {
        fprintf (stderr, "Bad power level.\n");
        return 0;
    }

    if (strcasecmp ("Wide", wide_str) == 0) {
        wide = 1;
    } else if (strcasecmp ("Narrow", wide_str) == 0) {
        wide = 0;
    } else {
        fprintf (stderr, "Bad modulation width.\n");
        return 0;
    }

    if (*scan_str == '+') {
        scan = 1;
    } else if (*scan_str == '-') {
        scan = 0;
    } else {
        fprintf (stderr, "Bad scan flag.\n");
        return 0;
    }

    if (*isam_str == '+') {
        isam = 1;
    } else if (*isam_str == '-') {
        isam = 0;
    } else {
        fprintf (stderr, "Bad BCL flag.\n");
        return 0;
    }

    if (*duplex_str == '+') {
        duplex = 1;
    } else if (*duplex_str == '-') {
        duplex = 0;
    } else {
        fprintf (stderr, "Bad duplex flag.\n");
        return 0;
    }

    if (first_row) {
        // On first entry, erase the channel table.
        int i;
        for (i=0; i<NCHAN; i++) {
            setup_channel (i, 0, 0, 0, 0, 1, 1, 0, 0, 0);
        }
    }
    setup_channel (num-1, rx_mhz, rx_mhz + txoff_mhz, rq, tq,
        highpower, wide, scan, isam, duplex);
#endif
    return 1;
}

//
// Yaesu FT-60R
//
radio_device_t radio_ft60r = {
    "Yaesu FT-60R",
    ft60r_download,
    ft60r_upload,
    ft60r_read_image,
    ft60r_save_image,
    ft60r_print_version,
    ft60r_print_config,
    ft60r_parse_parameter,
    ft60r_parse_header,
    ft60r_parse_row,
};
