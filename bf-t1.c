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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "radio.h"
#include "util.h"

#define NCHAN 24
#define MEMSZ 0x800
#define BLKSZ 16

static const char *OFF_ON[]     = { "Off", "On" };
static const char *LOW_HIGH[]   = { "Low", "High" };
static const char *LANGUAGE[]   = { "Off", "English", "Chinese" };
static const char *RELAY_MODE[] = { "Off", "Relay Receive", "Relay Send" };
static const char *BACKLIGHT[]  = { "Off", "Key Press", "Permanent" };
static const char *SCAN_MODE[]  = { "Timeout", "Carrier", "Search" };

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
        fprintf(stderr, "Bad reply for block 0x%04x of %d bytes: %02x-%02x-%02x-%02x\n", start,
                nbytes, reply[0], reply[1], reply[2], reply[3]);
        exit(-1);
    }

    // Read data.
    len = serial_read(fd, data, nbytes);
    if (len != nbytes) {
        fprintf(stderr, "Reading block 0x%04x: got only %d bytes.\n", start, len);
        exit(-1);
    }

    if (trace_flag) {
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

    if (trace_flag) {
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
    for (addr = 0; addr < MEMSZ; addr += BLKSZ)
        read_block(radio_port, addr, &radio_mem[addr], BLKSZ);
}

//
// Write memory image to the device.
//
static void bft1_upload(int cont_flag)
{
    int addr;
    unsigned char reply[1];

    for (addr = 0; addr < 0x180; addr += BLKSZ)
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
        *dcs  = 0;
        return;
    }
    // DCS mode.
    *dcs = DCS_CODES[index - 0x33];
    if (pol)
        *dcs = -*dcs;
    *ctcs = 0;
}

//
// Convert squelch string to polarity/tone value in BCD format.
// Four possible formats:
// nnn.n - CTCSS frequency
// DnnnN - DCS normal
// DnnnI - DCS inverted
// '-'   - Disabled
//
static int encode_squelch(char *str, int *pol)
{
    unsigned val;

    if (*str == 'D' || *str == 'd') {
        // DCS tone
        char *e;
        val = strtol(++str, &e, 10);

        // Find a valid index in DCS table.
        int i;
        for (i = 0; i < NDCS; i++)
            if (DCS_CODES[i] == val)
                break;
        if (i >= NDCS)
            return 0;

        val = i + 51;
        if (*e == 'N' || *e == 'n') {
            *pol = 0;
        } else if (*e == 'I' || *e == 'i') {
            *pol = 1;
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
        if (val < 0x0258)
            return 0;

        // Find a valid index in CTCSS table.
        int i;
        for (i = 0; i < NCTCSS; i++)
            if (CTCSS_TONES[i] == val)
                break;
        if (i >= NCTCSS)
            return 0;
        val  = i + 1;
        *pol = 0;
    } else {
        // Disabled
        return 0;
    }

    return val;
}

typedef struct {
    uint8_t rxfreq[4]; // binary coded decimal, 8 digits

    uint8_t rxtone; // x00 = none
                    // x01 - x32 = index of the analog tones
                    // x33 - x9b = index of Digital tones
                    // Digital tone polarity is handled below by
                    // ttondinv & ttondinv settings

    uint8_t txoffset[4]; // binary coded decimal, 8 digits
                         // the difference against RX, direction handled by
                         // offplus & offminus

    uint8_t txtone; // See rxtone

    uint8_t offminus : 1; // TX = RX - offset
    uint8_t offplus : 1;  // TX = RX + offset
    uint8_t _u1 : 1;
    uint8_t rtondinv : 1; // if true RX tone is Digital & Inverted
    uint8_t _u2 : 1;
    uint8_t ttondinv : 1; // if true TX tone is Digital & Inverted
    uint8_t wide : 1;     // 1 = Wide, 0 = Narrow
    uint8_t scan : 1;     // if true is included in the scan

    uint8_t _u3[5];
} memory_channel_t;

//
// Check whether the channel is defined.
//
static int channel_is_defined(memory_channel_t *ch)
{
    if (ch->rxfreq[0] == 0 && ch->rxfreq[1] == 0 && ch->rxfreq[2] == 0 && ch->rxfreq[3] == 0)
        return 0;
    if (ch->rxfreq[0] == 0xff && ch->rxfreq[1] == 0xff && ch->rxfreq[2] == 0xff &&
        ch->rxfreq[3] == 0xff)
        return 0;
    return 1;
}

static void decode_channel(int i, int *rx_hz, int *tx_hz, int *rx_ctcs, int *tx_ctcs, int *rx_dcs,
                           int *tx_dcs, int *wide, int *scan)
{
    memory_channel_t *ch = i + (memory_channel_t *)&radio_mem[0];

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

static void setup_channel(int i, double rx_mhz, double tx_mhz, int rq, int tq, int rpol, int tpol,
                          int wide, int scan)
{
    memory_channel_t *ch = i + (memory_channel_t *)&radio_mem[0x10];
    int txoff_mhz;

    int_to_bcd4(iround(rx_mhz * 100000.0 / 50) * 50, ch->rxfreq);

    if (rx_mhz == tx_mhz) {
        txoff_mhz    = 0;
        ch->offminus = 0;
        ch->offplus  = 0;
    } else if (tx_mhz > rx_mhz) {
        txoff_mhz    = tx_mhz - rx_mhz;
        ch->offminus = 0;
        ch->offplus  = 1;
    } else /*if (tx_mhz < rx_mhz)*/ {
        txoff_mhz    = rx_mhz - tx_mhz;
        ch->offminus = 1;
        ch->offplus  = 0;
    }

    int_to_bcd4(iround(txoff_mhz * 100000.0 / 50) * 50, ch->txoffset);
    ch->rxtone   = rq;
    ch->txtone   = tq;
    ch->wide     = wide;
    ch->scan     = scan;
    ch->rtondinv = rpol;
    ch->ttondinv = tpol;
    ch->_u1      = 0;
    ch->_u2      = 0;
    ch->_u3[0] = ch->_u3[1] = ch->_u3[2] = ch->_u3[3] = ch->_u3[4] = ~0;
}

static void print_offset(FILE *out, int delta)
{
    if (delta == 0) {
        fprintf(out, " 0      ");
    } else {
        if (delta > 0) {
            fprintf(out, "+");
        } else {
            fprintf(out, "-");
            delta = -delta;
        }
        if (delta % 1000000 == 0)
            fprintf(out, "%-7u", delta / 1000000);
        else
            fprintf(out, "%-7.3f", delta / 1000000.0);
    }
}

static void print_squelch(FILE *out, int ctcs, int dcs)
{
    if (ctcs)
        fprintf(out, "%5.1f", ctcs / 10.0);
    else if (dcs > 0)
        fprintf(out, "D%03dN", dcs);
    else if (dcs < 0)
        fprintf(out, "D%03dI", -dcs);
    else
        fprintf(out, "   - ");
}

//
// Settings at 0x150.
//
typedef struct {
    uint8_t vhfl[2]; // VHF low limit
    uint8_t vhfh[2]; // VHF high limit
    uint8_t uhfl[2]; // UHF low limit
    uint8_t uhfh[2]; // UHF high limit
    uint8_t _u0[8];
    uint8_t _u1[2];        // start of 0x0160
    uint8_t squelch;       // byte: 0-9
    uint8_t vox;           // byte: 0-9
    uint8_t timeout;       // tot, 0 off, then 30 sec increments up to 180
    uint8_t backlight : 2; // backlight 00 = off, 01 = key, 10 = on
    uint8_t lock : 1;      // keylock 0 = ff,  = on
    uint8_t beep : 1;      // key beep 0 = off, 1 = on
    uint8_t blo : 1;       // busy lockout 0 = off, 1 = on
    uint8_t ste : 1;       // squelch tail 0 = off, 1 = on
    uint8_t fm_funct : 1;  // fm-radio 0=off, 1=on ( off disables fm button on set )
    uint8_t batsave : 1;   // battery save 0 = off, 1 = on
    uint8_t scantype;      // scan type 0 = timed, 1 = carrier, 2 = stop
    uint8_t channel;       // active channel 1-20, setting it works on upload
    uint8_t fmrange;       // fm range 1 = low[65-76](ASIA), 0 = high[76-108](AMERICA)
    uint8_t alarm;         // alarm (count down timer)
                           //    d0 - d16 in half hour increments => off, 0.5 - 8.0 h
    uint8_t voice;         // voice prompt 0 = off, 1 = english, 2 = chinese
    uint8_t volume;        // volume 1-7 as per the radio steps
                           //    set to #FF by original software on upload
                           //    chirp uploads actual value and works.
    uint8_t fm_vfo[2];     // the frequency of the fm receiver.
                           //    resulting frequency is 65 + value * 0.1 MHz
                           //    0x145 is then 65 + 325*0.1 = 97.5 MHz
    uint8_t relaym;        // relay mode, d0 = off, d2 = re-tx, d1 = re-rx
                           //    still a mystery on how it works
    uint8_t tx_pwr;        // tx pwr 0 = low (0.5W), 1 = high(1.0W)
} settings_t;

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
    for (i = 0; i < NCHAN; i++) {
        int rx_hz, tx_hz, rx_ctcs, tx_ctcs, rx_dcs, tx_dcs;
        int wide, scan;

        if (i == 21 || i == 22)
            continue;

        decode_channel(i, &rx_hz, &tx_hz, &rx_ctcs, &tx_ctcs, &rx_dcs, &tx_dcs, &wide, &scan);
        if (rx_hz == 0) {
            // Channel is disabled
            continue;
        }

        fprintf(out, "%5d   ", i);
        if (rx_hz % 1000 != 0)
            fprintf(out, "%8.4f ", rx_hz / 1000000.0);
        else
            fprintf(out, "%7.3f  ", rx_hz / 1000000.0);
        print_offset(out, tx_hz - rx_hz);
        fprintf(out, " ");
        print_squelch(out, rx_ctcs, rx_dcs);
        fprintf(out, "   ");
        print_squelch(out, tx_ctcs, tx_dcs);

        fprintf(out, "   %-6s %s\n", wide ? "Wide" : "Narrow", scan ? "+" : "-");
    }
    if (verbose)
        print_squelch_tones(out, 0);

    // Print other settings.
    settings_t *mode = (settings_t *)&radio_mem[0x150];
    fprintf(out, "\n");

    // Current Channel
    if (verbose) {
        fprintf(out, "# Current selected channel.\n");
        fprintf(out, "# Options: 1, 2, ... 20\n");
    }
    fprintf(out, "Current Channel: %u\n", mode->channel);

    // Volume Level
    if (verbose) {
        fprintf(out, "\n# Audio volume level.\n");
        fprintf(out, "# Options: 1, 2, ... 7\n");
    }
    fprintf(out, "Volume Level: %u\n", mode->volume);

    // Transmit Power: Low, High
    if (verbose)
        print_options(out, LOW_HIGH, 2, "Transmit power.");
    fprintf(out, "Transmit Power: %s\n", mode->tx_pwr ? "High" : "Low");

    // Squelch Level: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
    if (verbose) {
        fprintf(out, "\n# Mute the speaker when a received signal is below this level.\n");
        fprintf(out, "# Options: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9\n");
    }
    fprintf(out, "Squelch Level: %u\n", mode->squelch);

    // Vox Level: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
    if (verbose) {
        fprintf(out, "\n# Voice operated transmission sensitivity.\n");
        fprintf(out, "# Options: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9\n");
    }
    fprintf(out, "VOX Level: %u\n", mode->vox);

    // Squelch Tail Eliminate: Off, On
    if (verbose)
        print_options(out, OFF_ON, 2,
                      "Reduce the squelch tail when communicating with simplex station.");
    fprintf(out, "Squelch Tail Eliminate: %s\n", mode->ste ? "On" : "Off");

    // Transmit Timer: Off, 30s, 60s, 90s, 120s, 150s, 180s
    if (verbose) {
        fprintf(out, "\n# Stop transmission after specified number of seconds.\n");
        fprintf(out, "# Options: Off, 30, 60, 90, 120, 150, 180\n");
    }
    fprintf(out, "Transmit Timer: ");
    if (mode->timeout == 0)
        fprintf(out, "Off\n");
    else
        fprintf(out, "%u\n", mode->timeout * 30);

    // Busy Channel Lockout: Off, On
    if (verbose)
        print_options(out, OFF_ON, 2, "Prevent transmission when a signal is received.");
    fprintf(out, "Busy Channel Lockout: %s\n", mode->blo ? "On" : "Off");

    // Scan Mode: Time, Carrier, Search
    if (verbose) {
        fprintf(out, "\n# Method of resuming the scan after stop on active channel.\n");
        fprintf(out, "# Timeout - resume after a few seconds.\n");
        fprintf(out, "# Carrier - resume after a carrier dropped off.\n");
        fprintf(out, "# Search - stop on next active frequency.\n");
    }
    fprintf(out, "Scan Resume: %s\n", SCAN_MODE[mode->scantype % 3]);

    // Alarm Timer: Off, 0.5h ... 8h
    if (verbose) {
        fprintf(out, "\n# Activate alarm after specified number of hours.\n");
        fprintf(out,
                "# Options: Off, 0.5, 1, 1.5, 2, 2.5, 3, 3.5, 4, 4.5, 5, 5.5, 6, 6.5, 7, 7.5, 8\n");
    }
    fprintf(out, "Alarm Timer: ");
    if (mode->alarm == 0)
        fprintf(out, "Off\n");
    else
        fprintf(out, "%d.%d\n", mode->alarm / 2, (mode->alarm & 1) ? 5 : 0);

    // Voice Prompt: Off, English, Chinese
    if (verbose)
        print_options(out, LANGUAGE, 3, "Enable voice messages, select the language.");
    fprintf(out, "Voice Prompt: %s\n", mode->voice < 3 ? LANGUAGE[mode->voice] : "???");

    // Key Beep: Off, On
    if (verbose)
        print_options(out, OFF_ON, 2, "Keypad beep sound.");
    fprintf(out, "Key Beep: %s\n", mode->beep ? "On" : "Off");

    // Key Lock: Off, On
    if (verbose)
        print_options(out, OFF_ON, 2, "Lock keypad.");
    fprintf(out, "Key Lock: %s\n", mode->lock ? "On" : "Off");

    // Battery Save: Off, On
    if (verbose)
        print_options(out, OFF_ON, 2, "Decrease the amount of power used when idle.");
    fprintf(out, "Battery Saver: %s\n", mode->batsave ? "On" : "Off");

    // Back Light: Off, Key Press, Permanent
    if (verbose)
        print_options(out, BACKLIGHT, 3, "Display backlight.");
    fprintf(out, "Back Light: %s\n", mode->backlight < 3 ? BACKLIGHT[mode->backlight] : "???");

    // FM Radio: Off, America, Asia
    if (verbose) {
        fprintf(out, "\n# Select FM radio mode.\n");
        fprintf(out, "# Options: Off, America, Asia\n");
        fprintf(out, "# Off - disable FM button\n");
        fprintf(out, "# America - 76-108 MHz\n");
        fprintf(out, "# Asia - 65-76 MHz\n");
    }
    fprintf(out, "FM Radio: %s\n",
            mode->fm_funct == 0  ? "Off"
            : mode->fmrange == 0 ? "America"
                                 : "Asia");

    // FM Frequency in MHz
    if (verbose) {
        fprintf(out, "\n# Current FM frequency in MHz.\n");
        fprintf(out, "# Options: 65.0 ... 108.0\n");
    }
    int mhz10 = mode->fm_vfo[0] * 256 + mode->fm_vfo[1] + 650;
    fprintf(out, "FM Frequency: %d.%d\n", mhz10 / 10, mhz10 % 10);

    // Relay Mode: Off, Relay Receive, Relay Send
    if (verbose)
        print_options(out, RELAY_MODE, 3, "Relay mode.");
    fprintf(out, "Relay Mode: %s\n", mode->relaym < 3 ? RELAY_MODE[mode->relaym] : "???");

    // VHF Range
    if (verbose) {
        fprintf(out, "\n# Frequency limits of VHF band in MHz.\n");
    }
    fprintf(out, "VHF Range: %d%d%d.%d-%d%d%d.%d\n", mode->vhfl[1] >> 4, mode->vhfl[1] & 15,
            mode->vhfl[0] >> 4, mode->vhfl[0] & 15, mode->vhfh[1] >> 4, mode->vhfh[1] & 15,
            mode->vhfh[0] >> 4, mode->vhfh[0] & 15);

    // UHF Range
    if (verbose) {
        fprintf(out, "\n# Frequency limits of UHF band in MHz.\n");
    }
    fprintf(out, "UHF Range: %d%d%d.%d-%d%d%d.%d\n", mode->uhfl[1] >> 4, mode->uhfl[1] & 15,
            mode->uhfl[0] >> 4, mode->uhfl[0] & 15, mode->uhfh[1] >> 4, mode->uhfh[1] & 15,
            mode->uhfh[0] >> 4, mode->uhfh[0] & 15);
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
    settings_t *mode = (settings_t *)&radio_mem[0x150];
    int i;

    if (strcasecmp("Radio", param) == 0) {
        if (strcasecmp("Baofeng BF-T1", value) != 0) {
        bad:
            fprintf(stderr, "Bad value for %s: %s\n", param, value);
            exit(-1);
        }
        return;
    }
    if (strcasecmp("Current Channel", param) == 0) {
        mode->channel = atoi(value);
        return;
    }
    if (strcasecmp("Volume Level", param) == 0) {
        mode->volume = atoi(value);
        return;
    }
    if (strcasecmp("Transmit Power", param) == 0) {
        if (strcasecmp("Low", value) == 0) {
            mode->tx_pwr = 0;
            return;
        }
        if (strcasecmp("High", value) == 0) {
            mode->tx_pwr = 1;
            return;
        }
        goto bad;
    }
    if (strcasecmp("Squelch Level", param) == 0) {
        mode->squelch = atoi(value);
        return;
    }
    if (strcasecmp("VOX Level", param) == 0) {
        mode->vox = atoi(value);
        return;
    }
    if (strcasecmp("Squelch Tail Eliminate", param) == 0) {
        mode->ste = on_off(param, value);
        return;
    }
    if (strcasecmp("Transmit Timer", param) == 0) {
        if (strcasecmp("Off", value) == 0) {
            mode->timeout = 0;
        } else {
            mode->timeout = atoi(value) / 30;
        }
        return;
    }
    if (strcasecmp("Busy Channel Lockout", param) == 0) {
        mode->blo = on_off(param, value);
        return;
    }
    if (strcasecmp("Scan Resume", param) == 0) {
        for (i = 0; i < 3; i++) {
            if (strcasecmp(SCAN_MODE[i], value) == 0) {
                mode->scantype = i;
                return;
            }
        }
        goto bad;
    }
    if (strcasecmp("Alarm Timer", param) == 0) {
        if (strcasecmp("Off", value) == 0) {
            mode->alarm = 0;
        } else {
            mode->alarm = atof(value) * 2 + 0.5;
        }
        return;
    }
    if (strcasecmp("Voice Prompt", param) == 0) {
        for (i = 0; i < 3; i++) {
            if (strcasecmp(LANGUAGE[i], value) == 0) {
                mode->voice = i;
                return;
            }
        }
        goto bad;
    }
    if (strcasecmp("Key Beep", param) == 0) {
        mode->beep = on_off(param, value);
        return;
    }
    if (strcasecmp("Key Lock", param) == 0) {
        mode->lock = on_off(param, value);
        return;
    }
    if (strcasecmp("Battery Saver", param) == 0) {
        mode->batsave = on_off(param, value);
        return;
    }
    if (strcasecmp("Back Light", param) == 0) {
        for (i = 0; i < 3; i++) {
            if (strcasecmp(BACKLIGHT[i], value) == 0) {
                mode->backlight = i;
                return;
            }
        }
        goto bad;
    }
    if (strcasecmp("FM Radio", param) == 0) {
        if (strcasecmp("Off", value) == 0) {
            mode->fm_funct = 0;
            return;
        }
        if (strcasecmp("America", value) == 0) {
            mode->fm_funct = 1;
            mode->fmrange  = 0;
            return;
        }
        if (strcasecmp("Asia", value) == 0) {
            mode->fm_funct = 1;
            mode->fmrange  = 1;
            return;
        }
        goto bad;
    }
    if (strcasecmp("FM Frequency", param) == 0) {
        int mhz10       = atof(value) * 10.0 + 0.5;
        mode->fm_vfo[0] = (mhz10 - 650) >> 8;
        mode->fm_vfo[1] = (mhz10 - 650);
        return;
    }
    if (strcasecmp("Relay Mode", param) == 0) {
        for (i = 0; i < 3; i++) {
            if (strcasecmp(RELAY_MODE[i], value) == 0) {
                mode->relaym = i;
                return;
            }
        }
        goto bad;
    }
    if (strcasecmp("VHF Range", param) == 0) {
        float upper, lower;
        if (sscanf(value, "%f-%f", &lower, &upper) != 2)
            goto bad;

        int lmhz10    = lower * 10.0 + 0.5;
        int umhz10    = upper * 10.0 + 0.5;
        mode->vhfl[1] = ((lmhz10 / 1000 % 10) << 4) | (lmhz10 / 100 % 10);
        mode->vhfl[0] = ((lmhz10 / 10 % 10) << 4) | (lmhz10 % 10);
        mode->vhfh[1] = ((umhz10 / 1000 % 10) << 4) | (umhz10 / 100 % 10);
        mode->vhfh[0] = ((umhz10 / 10 % 10) << 4) | (umhz10 % 10);
        return;
    }
    if (strcasecmp("UHF Range", param) == 0) {
        float upper, lower;
        if (sscanf(value, "%f-%f", &lower, &upper) != 2)
            goto bad;

        int lmhz10    = lower * 10.0 + 0.5;
        int umhz10    = upper * 10.0 + 0.5;
        mode->uhfl[1] = ((lmhz10 / 1000 % 10) << 4) | (lmhz10 / 100 % 10);
        mode->uhfl[0] = ((lmhz10 / 10 % 10) << 4) | (lmhz10 % 10);
        mode->uhfh[1] = ((umhz10 / 1000 % 10) << 4) | (umhz10 / 100 % 10);
        mode->uhfh[0] = ((umhz10 / 10 % 10) << 4) | (umhz10 % 10);
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
    int num, rq, tq, rpol, tpol, wide, scan;
    float rx_mhz, txoff_mhz;

    if (sscanf(line, "%s %s %s %s %s %s %s", num_str, rxfreq_str, offset_str, rq_str, tq_str,
               wide_str, scan_str) != 7)
        return 0;

    num = atoi(num_str);
    if (num < 0 || num > NCHAN || num == 21 || num == 22) {
        fprintf(stderr, "Bad channel number.\n");
        return 0;
    }
    if (sscanf(rxfreq_str, "%f", &rx_mhz) != 1 || !is_valid_frequency(rx_mhz)) {
        fprintf(stderr, "Bad receive frequency.\n");
        return 0;
    }
    if (sscanf(offset_str, "%f", &txoff_mhz) != 1 || !is_valid_frequency(rx_mhz + txoff_mhz)) {
        fprintf(stderr, "Bad transmit offset.\n");
        return 0;
    }
    rq = encode_squelch(rq_str, &rpol);
    tq = encode_squelch(tq_str, &tpol);

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
        memset(radio_mem, 0xff, 21 * 0x10);
        memset(&radio_mem[0x170], 0xff, 0x10);
    }
    setup_channel(num - 1, rx_mhz, rx_mhz + txoff_mhz, rq, tq, rpol, tpol, wide, scan);
    return 1;
}

//
// Baofeng BF-T1
//
radio_device_t radio_bft1 = {
    "Baofeng BF-T1",    bft1_download,     bft1_upload,          bft1_read_image,   bft1_save_image,
    bft1_print_version, bft1_print_config, bft1_parse_parameter, bft1_parse_header, bft1_parse_row,
};
