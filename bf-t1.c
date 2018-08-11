/*
 * Interface to Baofeng BF-T1 and compatibles.
 *
 * Copyright (C) 2018 Serge Vakulenko, KK6ABQ
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

#define NCHAN   24
#define MEMSZ   0x800
#define BLKSZ   16

//
// Print a generic information about the device.
//
static void bft1_print_version(FILE *out)
{
    // Nothing to print.
}

//
// Read block of data, up to 8 bytes.
// Halt the program on any error.
//
static void read_block(int fd, int start, unsigned char *data, int nbytes)
{
    unsigned char cmd[4], reply[4];
    int addr, len;

    // Send command.
    cmd[0] = 'R';
    cmd[1] = start >> 8;
    cmd[2] = start;
    cmd[3] = nbytes;
    serial_write(fd, cmd, 4);

    // Read reply.
    if (serial_read(fd, reply, 4) != 4) {
        fprintf(stderr, "Radio refused to send block 0x%04x.\n", start);
        exit(-1);
    }
    addr = reply[1] << 8 | reply[2];
    if (reply[0] != 'W' || addr != start || reply[3] != nbytes) {
        fprintf(stderr, "Bad reply for block 0x%04x of %d bytes: %02x-%02x-%02x-%02x\n",
            start, nbytes, reply[0], reply[1], reply[2], reply[3]);
        exit(-1);
    }

    // Read data.
    len = serial_read(fd, data, nbytes);
    if (len != nbytes) {
        fprintf(stderr, "Reading block 0x%04x: got only %d bytes.\n", start, len);
        exit(-1);
    }

    if (verbose) {
        printf("# Read 0x%04x: ", start);
        print_hex(data, nbytes);
        printf("\n");
    } else {
        ++radio_progress;
        if (radio_progress % 4 == 0) {
            fprintf(stderr, "#");
            fflush(stderr);
        }
    }
}

//
// Write block of data, up to 8 bytes.
// Halt the program on any error.
//
static void write_block(int fd, int start, const unsigned char *data, int nbytes)
{
    unsigned char cmd[4], reply;

    // Send command.
    cmd[0] = 'W';
    cmd[1] = start >> 8;
    cmd[2] = start;
    cmd[3] = nbytes;
    serial_write(fd, cmd, 4);
    serial_write(fd, data, nbytes);

    // Get acknowledge.
    if (serial_read(fd, &reply, 1) != 1) {
        fprintf(stderr, "No acknowledge after block 0x%04x.\n", start);
        exit(-1);
    }
    if (reply != 0x06) {
        fprintf(stderr, "Bad acknowledge after block 0x%04x: %02x\n", start, reply);
        exit(-1);
    }

    if (verbose) {
        printf("# Write 0x%04x: ", start);
        print_hex(data, nbytes);
        printf("\n");
    } else {
        ++radio_progress;
        if (radio_progress % 4 == 0) {
            fprintf(stderr, "#");
            fflush(stderr);
        }
    }
}

//
// Read memory image from the device.
//
static void bft1_download()
{
    int addr;

    memset(radio_mem, 0xff, MEMSZ);
    for (addr=0; addr<MEMSZ; addr+=BLKSZ)
        read_block(radio_port, addr, &radio_mem[addr], BLKSZ);
}

//
// Write memory image to the device.
//
static void bft1_upload(int cont_flag)
{
    int addr;
    unsigned char reply[1];

    for (addr=0; addr<0x180; addr+=BLKSZ)
        write_block(radio_port, addr, &radio_mem[addr], BLKSZ);

    // 'Bye'.
    serial_write(radio_port, "b", 1);
    if (serial_read(radio_port, reply, 1) != 1) {
        fprintf(stderr, "No acknowledge after upload.\n");
        exit(-1);
    }
    if (reply[0] != 0x00) {
        fprintf(stderr, "Bad acknowledge after upload: %02x\n", reply[0]);
        exit(-1);
    }
}

//
// x00 = none
// x01 - x32 = index of the analog tones
// x33 - x9b = index of Digital tones
//
static void decode_squelch(int index, int pol, int *ctcs, int *dcs)
{
    if (index == 0 || index > 0x9b) {
        // Squelch disabled.
        return;
    }

    if (index <= 0x32) {
        // CTCSS value is Hz multiplied by 10.
        *ctcs = CTCSS_TONES[index - 1];
        *dcs = 0;
        return;
    }
    // DCS mode.
    *dcs = DCS_CODES[index - 0x33];
    if (pol)
        *dcs = - *dcs;
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
static int encode_squelch(char *str)
{
    unsigned val;

    if (*str == 'D' || *str == 'd') {
        // DCS tone
        char *e;
        val = strtol(++str, &e, 10);
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
        if (sscanf(str, "%f", &hz) != 1)
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

typedef struct {
    uint8_t     rxfreq[4];      // binary coded decimal, 8 digits

    uint8_t     rxtone;         // x00 = none
                                // x01 - x32 = index of the analog tones
                                // x33 - x9b = index of Digital tones
                                // Digital tone polarity is handled below by
                                // ttondinv & ttondinv settings

    uint8_t     txoffset[4];    // binary coded decimal, 8 digits
                                // the difference against RX, direction handled by
                                // offplus & offminus

    uint8_t     txtone;         // See rxtone

    uint8_t     offminus : 1,   // TX = RX - offset
                offplus  : 1,   // TX = RX + offset
                _u1      : 1,
                rtondinv : 1,   // if true RX tone is Digital & Inverted
                _u2      : 1,
                ttondinv : 1,   // if true TX tone is Digital & Inverted
                wide     : 1,   // 1 = Wide, 0 = Narrow
                scan     : 1;   // if true is included in the scan

    uint8_t     _u3[5];
} memory_channel_t;

//
// Check whether the channel is defined.
//
static int channel_is_defined(memory_channel_t *ch)
{
    if (ch->rxfreq[0] == 0 && ch->rxfreq[1] == 0 && ch->rxfreq[2] == 0 && ch->rxfreq[3] == 0)
        return 0;
    if (ch->rxfreq[0] == 0xff && ch->rxfreq[1] == 0xff && ch->rxfreq[2] == 0xff && ch->rxfreq[3] == 0xff)
        return 0;
    return 1;
}

static void decode_channel(int i, int *rx_hz, int *tx_hz,
    int *rx_ctcs, int *tx_ctcs, int *rx_dcs, int *tx_dcs,
    int *wide, int *scan)
{
    memory_channel_t *ch = i + (memory_channel_t*) &radio_mem[0];

    *rx_hz = *tx_hz = *rx_ctcs = *tx_ctcs = *rx_dcs = *tx_dcs = 0;
    if (!channel_is_defined(ch))
        return;

    // Decode channel frequencies.
    *rx_hz = bcd4_to_int(ch->rxfreq) * 10;
    *tx_hz = bcd4_to_int(ch->txoffset) * 10;

    if (ch->offminus) {
        *tx_hz = *rx_hz - *tx_hz;
    } else if (ch->offplus) {
        *tx_hz = *rx_hz + *tx_hz;
    } else if (*tx_hz == 0) {
        *tx_hz = *rx_hz;
    }

    // Decode squelch modes.
    decode_squelch(ch->rxtone, ch->rtondinv, rx_ctcs, rx_dcs);
    decode_squelch(ch->txtone, ch->ttondinv, tx_ctcs, tx_dcs);

    // Other parameters.
    *wide = ch->wide;
    *scan = ch->scan;
}

//
// Round double value to integer.
//
static int iround(double x)
{
    if (x >= 0)
        return (int)(x + 0.5);

    return -(int)(-x + 0.5);
}

static void setup_channel(int i, double rx_mhz, double tx_mhz,
    int rq, int tq, int wide, int scan)
{
    memory_channel_t *ch = i + (memory_channel_t*) &radio_mem[0x10];

    int_to_bcd4(iround(rx_mhz * 100000.0), ch->rxfreq);
    int_to_bcd4(iround(tx_mhz * 100000.0), ch->txoffset);
    ch->rxtone = rq;
    ch->txtone = tq;
    ch->wide = wide;
    ch->scan = scan;
    ch->_u1 = 0;
    ch->_u2 = 0;
    ch->_u3[0] = ch->_u3[1] = ch->_u3[2] = ch->_u3[3] = ch->_u3[4] = ~0;
}

static void print_offset(FILE *out, int delta)
{
    if (delta == 0) {
        fprintf(out, " 0      ");
    } else {
        if (delta > 0) {
            fprintf(out, "+");;
        } else {
            fprintf(out, "-");;
            delta = - delta;
        }
        if (delta % 1000000 == 0)
            fprintf(out, "%-7u", delta / 1000000);
        else
            fprintf(out, "%-7.3f", delta / 1000000.0);
    }
}

static void print_squelch(FILE *out, int ctcs, int dcs)
{
    if      (ctcs)    fprintf(out, "%5.1f", ctcs / 10.0);
    else if (dcs > 0) fprintf(out, "D%03dN", dcs);
    else if (dcs < 0) fprintf(out, "D%03dI", -dcs);
    else              fprintf(out, "   - ");
}

//
// Print full information about the device configuration.
//
static void bft1_print_config(FILE *out, int verbose)
{
    int i;

    // Print memory channels.
    fprintf(out, "\n");
    if (verbose) {
        fprintf(out, "# Table of preprogrammed channels.\n");
        fprintf(out, "# 1) Channel number: 1-%d\n", NCHAN);
        fprintf(out, "# 2) Receive frequency in MHz\n");
        fprintf(out, "# 3) Offset of transmit frequency in MHz\n");
        fprintf(out, "# 4) Squelch tone for receive, or '-' to disable\n");
        fprintf(out, "# 5) Squelch tone for transmit, or '-' to disable\n");
        fprintf(out, "# 6) Modulation width: Wide, Narrow\n");
        fprintf(out, "# 7) Add this channel to scan list\n");
        fprintf(out, "#\n");
    }
    fprintf(out, "Channel Receive  TxOffset R-Squel T-Squel FM     Scan\n");
    for (i=0; i<NCHAN; i++) {
        int rx_hz, tx_hz, rx_ctcs, tx_ctcs, rx_dcs, tx_dcs;
        int wide, scan;

        if (i == 21 || i == 22)
            continue;

        decode_channel(i, &rx_hz, &tx_hz, &rx_ctcs, &tx_ctcs,
            &rx_dcs, &tx_dcs, &wide, &scan);
        if (rx_hz == 0) {
            // Channel is disabled
            continue;
        }

        fprintf(out, "%5d   %7.3f  ", i, rx_hz / 1000000.0);
        print_offset(out, tx_hz - rx_hz);
        fprintf(out, " ");
        print_squelch(out, rx_ctcs, rx_dcs);
        fprintf(out, "   ");
        print_squelch(out, tx_ctcs, tx_dcs);

        fprintf(out, "   %-6s %s\n",
            wide ? "Wide" : "Narrow", scan ? "+" : "-");
    }
    if (verbose)
        print_squelch_tones(out, 0);
}

//
// Read memory image from the binary file.
// Try to be compatible with Baofeng BF-480 software.
//
static void bft1_read_image(FILE *img, unsigned char *ident)
{
    if (fread(&radio_mem[0], 1, MEMSZ, img) != MEMSZ) {
        fprintf(stderr, "Error reading image data.\n");
        exit(-1);
    }
    memcpy(ident, " BF9100S", 8);
}

//
// Save memory image to the binary file.
// Try to be compatible with Baofeng BF-480 software.
//
static void bft1_save_image(FILE *img)
{
    fwrite(radio_mem, 1, MEMSZ, img);
}

static void bft1_parse_parameter(char *param, char *value)
{
    if (strcasecmp("Radio", param) == 0) {
        if (strcasecmp("Baofeng BF-T1", value) != 0) {
            fprintf(stderr, "Bad value for %s: %s\n", param, value);
            exit(-1);
        }
        return;
    }

    fprintf(stderr, "Unknown parameter: %s = %s\n", param, value);
    exit(-1);
}

//
// Check that the radio does support this frequency.
//
static int is_valid_frequency(int mhz)
{
    if (mhz >= 400 && mhz <= 470)
        return 1;
    return 0;
}

//
// Parse table header.
// Return table id, or 0 in case of error.
//
static int bft1_parse_header(char *line)
{
    if (strncasecmp(line, "Channel", 7) == 0)
        return 'C';

    return 0;
}

//
// Parse one line of table data.
// Start_flag is 1 for the first table row.
// Return 0 on failure.
//
static int bft1_parse_row(int table_id, int first_row, char *line)
{
    char num_str[256], rxfreq_str[256], offset_str[256], rq_str[256];
    char tq_str[256], wide_str[256], scan_str[256];
    int num, rq, tq, wide, scan;
    float rx_mhz, txoff_mhz;

    if (sscanf(line, "%s %s %s %s %s %s %s",
        num_str, rxfreq_str, offset_str, rq_str, tq_str, wide_str, scan_str) != 7)
        return 0;

    num = atoi(num_str);
    if (num < 1 || num > NCHAN) {
        fprintf(stderr, "Bad channel number.\n");
        return 0;
    }
    if (sscanf(rxfreq_str, "%f", &rx_mhz) != 1 ||
        ! is_valid_frequency(rx_mhz))
    {
        fprintf(stderr, "Bad receive frequency.\n");
        return 0;
    }
    if (sscanf(offset_str, "%f", &txoff_mhz) != 1 ||
        ! is_valid_frequency(rx_mhz + txoff_mhz))
    {
        fprintf(stderr, "Bad transmit offset.\n");
        return 0;
    }
    rq = encode_squelch(rq_str);
    tq = encode_squelch(tq_str);

    if (strcasecmp("Wide", wide_str) == 0) {
        wide = 1;
    } else if (strcasecmp("Narrow", wide_str) == 0) {
        wide = 0;
    } else {
        fprintf(stderr, "Bad modulation width.\n");
        return 0;
    }

    if (*scan_str == '+') {
        scan = 1;
    } else if (*scan_str == '-') {
        scan = 0;
    } else {
        fprintf(stderr, "Bad scan flag.\n");
        return 0;
    }

    if (first_row) {
        // On first entry, erase the channel table.
        int i;
        for (i=0; i<21; i++) {
            setup_channel(i, 0, 0, 0, 0, 1, 0);
        }
        setup_channel(23, 0, 0, 0, 0, 1, 0);
    }
    setup_channel(num-1, rx_mhz, rx_mhz + txoff_mhz, rq, tq, wide, scan);
    return 1;
}

//
// Baofeng BF-T1
//
radio_device_t radio_bft1 = {
    "Baofeng BF-T1",
    bft1_download,
    bft1_upload,
    bft1_read_image,
    bft1_save_image,
    bft1_print_version,
    bft1_print_config,
    bft1_parse_parameter,
    bft1_parse_header,
    bft1_parse_row,
};
