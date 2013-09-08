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

#define NCHAN           1000
#define MEMSZ           0x6fc8

#define OFFSET_VFO      0x0048
#define OFFSET_HOME     0x01c8
#define OFFSET_CHANNELS 0x0248
#define OFFSET_PMS      0x40c8
#define OFFSET_NAMES    0x4708
#define OFFSET_BANKS    0x69c8
#define OFFSET_SCAN     0x6ec8

static const char CHARSET[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ [?]^__|`?$%&-()*+,-,/|;/=>?@";
#define NCHARS  65
#define SPACE   36

static const char *BAND_NAME[5] = { "144", "250", "350", "430", "850" };

static const char *POWER_NAME[] = { "High", "Med", "Low", "??" };

static const char *SCAN_NAME[] = { "+", "Pref", "-", "??" };

//static const char *STEP_NAME[] = { "5.0", "10.0", "12.5", "15.0",
//                                   "20.0", "25.0", "50.0", "100.0" };

//
// Print a generic information about the device.
//
static void ft60r_print_version (FILE *out)
{
    // Nothing to print.
}

//
// Read block of data, up to 8 bytes.
// When start==0, return non-zero on success or 0 when empty.
// When start!=0, halt the program on any error.
//
static int read_block (int fd, int start, unsigned char *data, int nbytes)
{
    unsigned char reply;
    int len;

    // Read data.
    len = serial_read (fd, data, nbytes);
    if (len != nbytes) {
        if (start == 0)
            return 0;
        fprintf (stderr, "Reading block 0x%04x: got only %d bytes.\n", start, len);
        exit(-1);
    }

    // Get acknowledge.
    serial_write (fd, "\x06", 1);
    if (serial_read (fd, &reply, 1) != 1) {
        fprintf (stderr, "No acknowledge after block 0x%04x.\n", start);
        exit(-1);
    }
    if (reply != 0x06) {
        fprintf (stderr, "Bad acknowledge after block 0x%04x: %02x\n", start, reply);
        exit(-1);
    }
    if (verbose) {
        printf ("# Read 0x%04x: ", start);
        print_hex (data, nbytes);
        printf ("\n");
    } else {
        ++radio_progress;
        if (radio_progress % 16 == 0) {
            fprintf (stderr, "#");
            fflush (stderr);
        }
    }
    return 1;
}

//
// Write block of data, up to 8 bytes.
// Halt the program on any error.
//
static void write_block (int fd, int start, const unsigned char *data, int nbytes)
{
#if 0
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
        if (radio_progress % 16 == 0) {
            fprintf (stderr, "#");
            fflush (stderr);
        }
    }
#endif
}

//
// Read memory image from the device.
//
static void ft60r_download()
{
    int addr, sum;

    if (verbose)
        fprintf (stderr, "\nPlease follow the procedure:\n");
    else
        fprintf (stderr, "please follow the procedure.\n");
    fprintf (stderr, "\n");
    fprintf (stderr, "1. Power Off the FT60.\n");
    fprintf (stderr, "2. Hold down the MONI switch and Power On the FT60.\n");
    fprintf (stderr, "3. Rotate the right DIAL knob to select F8 CLONE.\n");
    fprintf (stderr, "4. Briefly press the [F/W] key. The display should go blank then show CLONE.\n");
    fprintf (stderr, "5. Press and hold the PTT switch until the radio starts to send.\n");
    fprintf (stderr, "-- Or enter ^C to abort the memory read.\n");
again:
    fprintf (stderr, "\n");
    fprintf (stderr, "Waiting for data... ");
    fflush (stderr);

    // Wait for the first 8 bytes.
    while (read_block (radio_port, 0, radio_ident, 8) == 0)
        continue;

    // Get the rest of data.
    for (addr=8; addr<MEMSZ; addr+=64)
        read_block (radio_port, addr, &radio_mem[addr], 64);

    // Get the checksum.
    read_block (radio_port, MEMSZ, &radio_mem[MEMSZ], 1);

    // Verify the checksum.
    sum = 0;
    for (addr=0; addr<8; addr++)
        sum += radio_ident[addr];
    for (addr=8; addr<MEMSZ; addr++)
        sum += radio_mem[addr];
    sum = sum & 0xff;
    if (sum != radio_mem[MEMSZ]) {
        if (verbose) {
            printf ("Checksum = %02x (BAD)\n", radio_mem[MEMSZ]);
            fprintf (stderr, "BAD CHECKSUM!\n");
        } else
            fprintf (stderr, "[BAD CHECKSUM]\n");
        fprintf (stderr, "Please, repeat the procedure:\n");
        fprintf (stderr, "Press and hold the PTT switch until the radio starts to send.\n");
        fprintf (stderr, "Or enter ^C to abort the memory read.\n");
        goto again;
    }
    if (verbose)
        printf ("Checksum = %02x (OK)\n", radio_mem[MEMSZ]);
}

//
// Write memory image to the device.
//
static void ft60r_upload()
{
    int addr;

    write_block (radio_port, 0, radio_ident, 8);
    for (addr=8; addr<MEMSZ; addr+=64)
        write_block (radio_port, addr, &radio_mem[addr], 64);
    // TODO: checksum
    write_block (radio_port, MEMSZ, &radio_mem[MEMSZ], 1);
}

#if 0
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
    uint8_t     duplex    : 4,  // Repeater mode
#define D_SIMPLEX       0
#define D_NEG_OFFSET    2
#define D_POS_OFFSET    3
#define D_CROSS_BAND    4
                isam      : 1,  // Amplitude modulation
                isnarrow  : 1,  // Narrow FM modulation
                _u1       : 1,
                used      : 1;  // Channel is used
    uint8_t     rxfreq [3];     // Receive frequency
    uint8_t     tmode     : 3,  // CTCSS/DCS mode
#define T_OFF           0
#define T_TONE          1
#define T_TSQL          2
#define T_TSQL_REV      3
#define T_DTCS          4
#define T_D             5
#define T_T_DCS         6
#define T_D_TSQL        7
                step      : 3,  // Frequency step
                _u2       : 2;
    uint8_t     txfreq [3];     // Transmit frequency when cross-band
    uint8_t     tone      : 6,  // CTCSS tone select
                power     : 2;  // Transmit power level
    uint8_t     dtcs      : 7,  // DCS code select
                _u3       : 1;
    uint8_t     _u4 [2];
    uint8_t     offset;         // TX offset, in 50kHz steps
    uint8_t     _u5 [3];
} memory_channel_t;

typedef struct {
    uint8_t     name[6];
    uint8_t     _u1       : 7,
                used      : 1;
    uint8_t     _u2       : 7,
                valid     : 1;
} memory_name_t;

#if 0
80 043000 50 000000 0c 00 0f00 64 000000 // 430.000
83 044310 50 000000 0c 00 0f00 64 000000 // 443.100 +
83 844312 50 000000 0c 00 0f00 64 000000 // 443.125 +
80 c16293 21 000000 0c 00 0f00 0c 000000 // 162.9375 tone 100.0
#endif

//
// Convert 32-bit value from binary coded decimal
// to integer format (8 digits).
//
int freq_to_hz (uint8_t *bcd)
{
    int hz;

    hz = (bcd[0]       & 15) * 100000000 +
        ((bcd[1] >> 4) & 15) * 10000000 +
         (bcd[1]       & 15) * 1000000 +
        ((bcd[2] >> 4) & 15) * 100000 +
         (bcd[2]       & 15) * 10000;
    hz += (bcd[0] >> 6) * 2500;
    return hz;
}

static int decode_banks (int i)
{
    int b, mask, data;

    mask = 0;
    for (b=0; b<10; b++) {
        data = radio_mem [OFFSET_BANKS + b * 0x80 + i/8];
        if ((data >> (i & 7)) & 1)
            mask |= 1 << b;
    }
    return mask;
}

static void decode_channel (int i, int seek, char *name,
    int *rx_hz, int *tx_hz, int *rx_ctcs, int *tx_ctcs,
    int *rx_dcs, int *tx_dcs, int *power, int *wide,
    int *scan, int *isam, int *step, int *banks)
{
    memory_channel_t *ch = i + (memory_channel_t*) &radio_mem[seek];
    int scan_data = radio_mem[OFFSET_SCAN + i/4];

    *rx_hz = *tx_hz = *rx_ctcs = *tx_ctcs = *rx_dcs = *tx_dcs = 0;
    *power = *wide = *scan = *isam = *step = 0;
    if (name)
        *name = 0;
    if (banks)
        *banks = 0;
    if (! ch->used && (seek == OFFSET_CHANNELS || seek == OFFSET_PMS))
        return;

    // Extract channel name; strip trailing FF's.
    if (name && seek == OFFSET_CHANNELS) {
        memory_name_t *nm = i + (memory_name_t*) &radio_mem[OFFSET_NAMES];
        if (nm->valid && nm->used) {
            int n, c;
            for (n=0; n<6; n++) {
                c = nm->name[n];
                name[n] = (c < NCHARS) ? CHARSET[c] : SPACE;
            }
            name[6] = 0;
        }
    }

    // Decode channel frequencies.
    *rx_hz = freq_to_hz (ch->rxfreq);

    *tx_hz = *rx_hz;
    switch (ch->duplex) {
    case D_NEG_OFFSET:
        *tx_hz -= ch->offset * 50000;
        break;
    case D_POS_OFFSET:
        *tx_hz += ch->offset * 50000;
        break;
    case D_CROSS_BAND:
        *tx_hz = freq_to_hz (ch->txfreq);
        break;
    }

    // Decode squelch modes.
    switch (ch->tmode) {
    case T_TONE:
        *tx_ctcs = CTCSS_TONES[ch->tone];
        break;
    case T_TSQL:
        *tx_ctcs = CTCSS_TONES[ch->tone];
        *rx_ctcs = CTCSS_TONES[ch->tone];
        break;
    case T_TSQL_REV:
        *tx_ctcs = CTCSS_TONES[ch->tone];
        *rx_ctcs = - CTCSS_TONES[ch->tone];
        break;
    case T_DTCS:
        *tx_dcs = DCS_CODES[ch->dtcs];
        *rx_dcs = DCS_CODES[ch->dtcs];
        break;
    case T_D:
        *tx_dcs = DCS_CODES[ch->dtcs];
        break;
    case T_T_DCS:
        *tx_ctcs = CTCSS_TONES[ch->tone];
        *rx_dcs = DCS_CODES[ch->dtcs];
        break;
    case T_D_TSQL:
        *tx_dcs = DCS_CODES[ch->dtcs];
        *rx_ctcs = CTCSS_TONES[ch->tone];
        break;
    }

    // Other parameters.
    *power = ch->power;
    *wide = ! ch->isnarrow;
    *scan = (scan_data << ((i & 3) * 2) >> 6) & 3;
    *isam = ch->isam;
    *step = ch->step;

    if (seek == OFFSET_CHANNELS)
        *banks = decode_banks (i);;
}

#if 0
static void setup_channel (int i, char *name, double rx_mhz, double tx_mhz,
    int rq, int tq, int power, int wide, int scan, int isam, int step, int banks)
{
    memory_channel_t *ch = i + (memory_channel_t*) &radio_mem[0x10];

    ch->rxfreq = int_to_bcd ((int) (rx_mhz * 100000.0));
    ch->txfreq = int_to_bcd ((int) (tx_mhz * 100000.0));
    ch->rxtone = rq;
    ch->txtone = tq;
    ch->power = power;
    ch->isnarrow = ! wide;
    ch->scan = scan;
    ch->isam = isam;
    ch->step = step;
    // TODO: banks

    // TODO: Copy channel name.
    strncpy ((char*) &radio_mem[0x1000 + i*16], name, 7);
}
#endif

static void print_offset (FILE *out, int rx_hz, int tx_hz)
{
    int delta = tx_hz - rx_hz;

    if (delta == 0) {
        fprintf (out, "+0      ");
    } else if (delta > 0 && delta/50000 <= 255) {
        if (delta % 1000000 == 0)
            fprintf (out, "+%-7u", delta / 1000000);
        else
            fprintf (out, "+%-7.3f", delta / 1000000.0);
    } else if (delta < 0 && -delta/50000 <= 255) {
        delta = - delta;
        if (delta % 1000000 == 0)
            fprintf (out, "-%-7u", delta / 1000000);
        else
            fprintf (out, "-%-7.3f", delta / 1000000.0);
    } else {
        // Cross band mode.
        fprintf (out, " %-7.4f", tx_hz / 1000000.0);
    }
}

static void print_squelch (FILE *out, int ctcs, int dcs)
{
    if      (ctcs)    fprintf (out, "%5.1f", ctcs / 10.0);
    else if (dcs > 0) fprintf (out, "D%03d", dcs);
    else              fprintf (out, "   - ");
}

static char *format_banks (int mask)
{
    static char buf [16];
    char *p;
    int b;

    p = buf;
    for (b=0; b<10; b++) {
        if ((mask >> b) & 1)
            *p++ = "1234567890" [b];
    }
    if (p == buf)
        *p++ = '-';
    *p = 0;
    return buf;
}

//
// Print full information about the device configuration.
//
static void ft60r_print_config (FILE *out, int verbose)
{
    int i;

    //
    // Memory channels.
    //
    fprintf (out, "\n");
    if (verbose) {
        fprintf (out, "# Table of preprogrammed channels.\n");
        fprintf (out, "# 1) Channel number: 1-%d\n", NCHAN);
        fprintf (out, "# 2) Receive frequency in MHz\n");
        fprintf (out, "# 3) Transmit frequency or offset in MHz\n");
        fprintf (out, "# 4) Squelch tone for receive, or '-' to disable\n");
        fprintf (out, "# 5) Squelch tone for transmit, or '-' to disable\n");
        fprintf (out, "# 6) Transmit power: High, Mid, Low\n");
        fprintf (out, "# 7) Modulation: Wide, Narrow, AM\n");
        fprintf (out, "# 8) Scan mode: +, -, Pref\n");
        //fprintf (out, "#\n");
    }
    fprintf (out, "Channel Name    Receive  Transmit R-Squel T-Squel Power Modulation Scan Banks\n");
    for (i=0; i<NCHAN; i++) {
        int rx_hz, tx_hz, rx_ctcs, tx_ctcs, rx_dcs, tx_dcs;
        int power, wide, scan, isam, step, banks;
        char name[17];

        decode_channel (i, OFFSET_CHANNELS, name, &rx_hz, &tx_hz, &rx_ctcs, &tx_ctcs,
            &rx_dcs, &tx_dcs, &power, &wide, &scan, &isam, &step, &banks);
        if (rx_hz == 0) {
            // Channel is disabled
            continue;
        }

        fprintf (out, "%5d   %-7s %8.4f ", i+1, name, rx_hz / 1000000.0);
        print_offset (out, rx_hz, tx_hz);
        fprintf (out, " ");
        print_squelch (out, rx_ctcs, rx_dcs);
        fprintf (out, "   ");
        print_squelch (out, tx_ctcs, tx_dcs);

        fprintf (out, "   %-4s  %-10s %-4s %s\n", POWER_NAME[power],
            isam ? "AM" : wide ? "Wide" : "Narrow", SCAN_NAME[scan],
            format_banks (banks));
    }
    //if (verbose)
    //    print_squelch_tones (out);

    //
    // Preferred memory scans.
    //
    fprintf (out, "\n");
    fprintf (out, "PMS     Lower    Upper\n");
    for (i=0; i<50; i++) {
        int lower_hz, upper_hz, tx_hz, rx_ctcs, tx_ctcs, rx_dcs, tx_dcs;
        int power, wide, scan, isam, step;

        decode_channel (i*2, OFFSET_PMS, 0, &lower_hz, &tx_hz, &rx_ctcs, &tx_ctcs,
            &rx_dcs, &tx_dcs, &power, &wide, &scan, &isam, &step, 0);
        decode_channel (i*2+1, OFFSET_PMS, 0, &upper_hz, &tx_hz, &rx_ctcs, &tx_ctcs,
            &rx_dcs, &tx_dcs, &power, &wide, &scan, &isam, &step, 0);
        if (lower_hz == 0 && upper_hz == 0)
            continue;

        fprintf (out, "%5d   ", i+1);
        if (lower_hz == 0)
            fprintf (out, "-       ");
        else
            fprintf (out, "%8.4f", lower_hz / 1000000.0);
        if (upper_hz == 0)

            fprintf (out, " -\n");
        else
            fprintf (out, " %8.4f\n", upper_hz / 1000000.0);
    }

    //
    // Home channels.
    //
    fprintf (out, "\n");
    fprintf (out, "Home    Receive  Transmit R-Squel T-Squel Power Modulation\n");
    for (i=0; i<5; i++) {
        int rx_hz, tx_hz, rx_ctcs, tx_ctcs, rx_dcs, tx_dcs;
        int power, wide, scan, isam, step;

        decode_channel (i, OFFSET_HOME, 0, &rx_hz, &tx_hz, &rx_ctcs, &tx_ctcs,
            &rx_dcs, &tx_dcs, &power, &wide, &scan, &isam, &step, 0);

        fprintf (out, "%5s   %8.4f ", BAND_NAME[i], rx_hz / 1000000.0);
        print_offset (out, rx_hz, tx_hz);
        fprintf (out, " ");
        print_squelch (out, rx_ctcs, rx_dcs);
        fprintf (out, "   ");
        print_squelch (out, tx_ctcs, tx_dcs);

        fprintf (out, "   %-4s  %s\n", POWER_NAME[power],
            isam ? "AM" : wide ? "Wide" : "Narrow");
    }

    //
    // VFO channels.
    // Not much sense to store this to the configuration file.
    //
#if 0
    fprintf (out, "\n");
    fprintf (out, "VFO     Receive  Transmit R-Squel T-Squel Step  Power Modulation\n");
    for (i=0; i<5; i++) {
        int rx_hz, tx_hz, rx_ctcs, tx_ctcs, rx_dcs, tx_dcs;
        int power, wide, scan, isam, step;

        decode_channel (i, OFFSET_VFO, &rx_hz, &tx_hz, &rx_ctcs, &tx_ctcs,
            &rx_dcs, &tx_dcs, &power, &wide, &scan, &isam, &step, 0);

        fprintf (out, "%5s   %8.4f ", BAND_NAME[i], rx_hz / 1000000.0);
        print_offset (out, rx_hz, tx_hz);
        fprintf (out, " ");
        print_squelch (out, rx_ctcs, rx_dcs);
        fprintf (out, "   ");
        print_squelch (out, tx_ctcs, tx_dcs);

        fprintf (out, "   %-5s %-4s  %s\n",
            STEP_NAME[step], POWER_NAME[power],
            isam ? "AM" : wide ? "Wide" : "Narrow");
    }
#endif
}

//
// Read memory image from the binary file.
//
static void ft60r_read_image (FILE *img, unsigned char *ident)
{
    if (fread (&radio_mem[0], 1, MEMSZ, img) != MEMSZ) {
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
    memcpy (radio_mem, radio_ident, 8);
    fwrite (&radio_mem[0], 1, MEMSZ, img);
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
// Parse one line of table data.
// Start_flag is 1 for the first table row.
// Return 0 on failure.
//
static int parse_channel (int first_row, char *line)
{
    char num_str[256], name_str[256], rxfreq_str[256], offset_str[256];
    char rq_str[256], tq_str[256], power_str[256], wide_str[256];
    char scan_str[256], banks_str[256];
    int num; // rq, tq, power, wide, scan, isam, step;
    //float rx_mhz, txoff_mhz;

    if (sscanf (line, "%s %s %s %s %s %s %s %s %s %s",
        num_str, name_str, rxfreq_str, offset_str, rq_str, tq_str, power_str,
        wide_str, scan_str, banks_str) != 10)
        return 0;

    num = atoi (num_str);
    if (num < 1 || num > NCHAN) {
        fprintf (stderr, "Bad channel number.\n");
        return 0;
    }
    // TODO
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
        power = 0;
    } else if (strcasecmp ("Mid", power_str) == 0) {
        power = 1;
    } else if (strcasecmp ("Low", power_str) == 0) {
        power = 2;
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

    if (*banks_str == '+') {
        banks = 1;
    } else if (*banks_str == '-') {
        banks = 0;
    } else {
        fprintf (stderr, "Bad banks mask.\n");
        return 0;
    }

    if (first_row) {
        // On first entry, erase the channel table.
        int i;
        for (i=0; i<NCHAN; i++) {
            setup_channel (i, 0, 0, 0, 0, 1, 1, 0, 0, 0);
        }
    }
    setup_channel (num-1, name_str, rx_mhz, rx_mhz + txoff_mhz,
        rq, tq, power, wide, scan, isam, step);
#endif
    return 1;
}

static int parse_home (int first_row, char *line)
{
    // TODO
    return 1;
}

static int parse_pms (int first_row, char *line)
{
    // TODO
    return 1;
}

//
// Parse table header.
// Return table id, or 0 in case of error.
//
static int ft60r_parse_header (char *line)
{
    if (strncasecmp (line, "Channel", 7) == 0)
        return 'C';
    if (strncasecmp (line, "Home", 4) == 0)
        return 'H';
    if (strncasecmp (line, "PMS", 3) == 0)
        return 'P';
    return 0;
}

static int ft60r_parse_row (int table_id, int first_row, char *line)
{
    switch (table_id) {
    case 'C': return parse_channel (first_row, line);
    case 'H': return parse_home (first_row, line);
    case 'P': return parse_pms (first_row, line);
    }
    return 0;
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
