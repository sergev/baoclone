//
// Tests for command line interface.
//
// Copyright (c) 2023 Serge Vakulenko
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#include <cstdio>
#include <fstream>
#include <stdio.h>

#include "util.h"
#include "radio.h"

//
// Show configuration from image file, located in the examples directory.
//
static std::string show_config(const std::string &img_basename)
{
    std::string base_name       = get_test_name();
    std::string output_filename = base_name + ".conf";
    std::string img_filename    = std::string(TEST_DIR "/../examples/") + img_basename;

    FILE *output = fopen(output_filename.c_str(), "w");
    EXPECT_NE(output, nullptr);

    radio_read_image(img_filename.c_str());
    radio_print_config(output, false);
    fclose(output);
    return file_contents(output_filename);
}

TEST(config, uv_5r_factory)
{
    auto result = show_config("uv-5r-factory.img");

    EXPECT_EQ(result, R"(Message: AAAAAAABBBBBBB

Channel Name    Receive  TxOffset R-Squel T-Squel Power FM     Scan BCL Scode PTTID
    0   -       136.0250  0          -       -    High  Wide   +    -   -     -
  127   -       470.6250  0          -       -    High  Wide   +    -   -     -

VFO Band Receive  TxOffset R-Squel T-Squel Step Power FM     Scode
 A  VHF  136.0250  0          -       -    20.0 High  Wide   -
 B  UHF  470.6250  0          -       -    20.0 High  Wide   -

Limit Lower Upper Enable
 VHF   136   174  +
 UHF   400   520  +

Squelch Level: 4
Battery Saver: 3
VOX Level: Off
Backlight Timeout: 5
Dual Watch: Off
Keypad Beep: On
TX Timer: 60
Voice Prompt: English
ANI Code: 80808
DTMF Sidetone: DTMF+ANI
Scan Resume: TO
Display Mode A: Frequency
Display Mode B: Frequency
Busy Channel Lockout: Off
Auto Key Lock: Off
Standby LED Color: Purple
RX LED Color: Blue
TX LED Color: Orange
Alarm Mode: Site
Squelch Tail Eliminate: Off
Squelch Tail Eliminate for Repeater: Off
Squelch Tail Repeater Delay: Off
Power-On Message: Off
Roger Beep: Off
)");
}

TEST(config, bf_888s_factory)
{
    auto result = show_config("bf-888s-factory.img");

    EXPECT_EQ(result, R"(
Channel Receive  TxOffset R-Squel T-Squel Power FM     Scan BCL Scramble
    1   462.1250  0        69.3    69.3   High  Wide   -    -   -
    2   462.2250  0          -       -    High  Wide   -    -   -
    3   462.3250  0          -       -    High  Wide   -    -   -
    4   462.4250  0       103.5   103.5   High  Wide   -    -   -
    5   462.5250  0       114.8   114.8   High  Wide   -    -   -
    6   462.6250  0       127.3   127.3   High  Wide   -    -   -
    7   462.7250  0       136.5   136.5   High  Wide   -    -   -
    8   462.8250  0       162.2   162.2   High  Wide   -    -   -
    9   462.9250  0       D025N   D025N   High  Wide   -    -   -
   10   463.0250  0       D051N   D051N   High  Wide   -    -   -
   11   463.1250  0       D125N   D125N   High  Wide   -    -   -
   12   463.2250  0       D155I   D155I   High  Wide   -    -   -
   13   463.5250  0       D465I   D465I   High  Wide   -    -   -
   14   450.2250  0       D023N   D023N   High  Wide   -    -   -
   15   460.3250  0          -       -    High  Wide   -    -   -
   16   469.9500  0       203.5   203.5   High  Wide   -    -   -

Squelch Level: 5
Side Key: Monitor
TX Timer: 300
Scan Function: Off
Voice Prompt: On
Voice Language: English
Alarm: Off
FM Radio: Off
VOX Function: Off
VOX Level: 3
VOX Inhibit On Receive: Off
Battery Saver: On
Beep: On
High Vol Inhibit TX: On
Low Vol Inhibit TX: On
)");
}

TEST(config, bf_888s_v2_factory)
{
    auto result = show_config("bf-888s-factory2.img");

    EXPECT_EQ(result, R"(
Channel Receive  TxOffset R-Squel T-Squel Power FM     Scan BCL Scramble
    1   462.1250  0        69.3    69.3   High  Wide   -    -   -
    2   462.2250  0          -       -    High  Wide   -    -   -
    3   462.3250  0          -       -    High  Wide   -    -   -
    4   462.4250  0       103.5   103.5   High  Wide   -    -   -
    5   462.5250  0       114.8   114.8   High  Wide   -    -   -
    6   462.6250  0       127.3   127.3   High  Wide   -    -   -
    7   462.7250  0       136.5   136.5   High  Wide   -    -   -
    8   462.8250  0       162.2   162.2   High  Wide   -    -   -
    9   462.9250  0       D025N   D025N   High  Wide   -    -   -
   10   463.0250  0       D051N   D051N   High  Wide   -    -   -
   11   463.1250  0       D125N   D125N   High  Wide   -    -   -
   12   463.2250  0       D155I   D155I   High  Wide   -    -   -
   13   463.5250  0       D465I   D465I   High  Wide   -    -   -
   14   450.2250  0       D023N   D023N   High  Wide   -    -   -
   15   460.3250  0          -       -    High  Wide   -    -   -
   16   469.9500  0       203.5   203.5   High  Wide   +    -   -

Squelch Level: 3
Side Key: Monitor
TX Timer: 180
Scan Function: On
Voice Prompt: On
Voice Language: English
Alarm: Off
FM Radio: On
VOX Function: Off
VOX Level: 3
VOX Inhibit On Receive: Off
Battery Saver: On
Beep: On
High Vol Inhibit TX: On
Low Vol Inhibit TX: On
)");
}

TEST(config, uv_b5_factory)
{
    auto result = show_config("uv-b5-factory.img");

    EXPECT_EQ(result, R"(
Channel Name   Receive  TxOffset Rx-Sq Tx-Sq Power FM   Scan PTTID BCL Rev Compand
    1   ONE    136.0000  0          -     -  High  Wide   +    -    -   -    -
    2   TWO    453.2250  0        91.5  91.5 High  Wide   -    -    -   -    -
    3   THREE  454.3250  0       136.5 136.5 High  Wide   -    -    -   -    -
    4   ROUR   455.4250  0       151.4 151.4 High  Wide   -    -    -   -    -
    5   FIVE   456.5250  0       192.8 192.8 High  Wide   -    -    -   -    -
    6   SIX    457.6250  0       241.8 241.8 High  Wide   -    -    -   -    -
    7   SEVEN  441.8400  0          -     -  High  Wide   +    -    -   -    -
    8   EIGHT  459.8250  0       D134N D134N High  Wide   -    -    -   -    -
    9   NIME   461.9250  0       D274N D274N High  Wide   -    -    -   -    -
   10   TEN    462.2250  0       D346N D346N High  Wide   -    -    -   -    -
   11   ELEVE  463.3250  0       D503N D503N High  Wide   -    -    -   -    -
   12   -      464.4250  0       D073I D073I High  Wide   -    -    -   -    -
   13   -      465.5250  0       D703I D703I High  Wide   -    -    -   -    -
   14   -      402.2250  0          -     -  High  Wide   -    -    -   -    -
   15   -      437.4250  0          -     -  High  Wide   -    -    -   -    -
   16   -      469.9750  0          -     -  High  Wide   -    -    -   -    -
   17   -      138.5500  0          -     -  High  Wide   -    -    -   -    -
   18   -      157.6500  0          -     -  High  Wide   -    -    -   -    -
   19   -      172.7500  0          -     -  High  Wide   -    -    -   -    -
   20   -      438.5000  0          -     -  High  Wide   -    -    -   -    -
   21   -      155.7000  0          -     -  High  Wide   -    -    -   -    -

VFO Receive  TxOffset Rx-Sq Tx-Sq Step Power FM   PTTID BCL Rev Compand
 A  443.0750  0          -     -  25.0 High  Wide   -    -   -    -
 B  145.2300  0          -     -  5.0  High  Wide   -    -   -    -

Limit Lower  Upper
 VHF  136.0  174.0
 UHF  400.0  480.0

FM   Frequency
 1    91.5
 2    92.3
 3    94.1
 4    95.3
 5    96.1
 6    97.7
 7    98.5
 8    99.3
 9   100.3
 10  100.9
 11  102.9
 12  103.3
 13  104.9
 14  105.7
 15  106.5
 16  107.7

Squelch Level: 1
Battery Saver: On
Roger Beep: Off
TX Timer: 2
VOX Level: Off
Keypad Beep: On
Voice Prompt: On
Dual Watch: Off
Backlight: On
PTT ID Transmit: Off
ANI Code: 808080
DTMF Sidetone: On
Display A Mode: Frequency
Display B Mode: Frequency
Scan Resume: Time
TX Dual Watch: Current Frequency
Squelch Tail Eliminate: On
Voice Language: English
)");
}

TEST(config, bf_t1_factory)
{
    auto result = show_config("bf-t1-factory.img");

    EXPECT_EQ(result, R"(
Channel Receive  TxOffset R-Squel T-Squel FM     Scan
    0   442.525   0        88.5    88.5   Wide   -
    1   437.150   0        69.3    69.3   Wide   -
    2   439.250   0       100.0   100.0   Wide   -
    3   441.750   0       151.4   151.4   Wide   -
    4   443.450   0       203.5   203.5   Wide   -
    5   445.550   0       241.8   241.8   Wide   -
    6   447.650   0       D023N   D023N   Wide   -
    7   449.750   0       D114N   D114N   Wide   -
    8   438.650   0       D205N   D205N   Wide   -
    9   442.350   0       D306N   D306N   Wide   -
   10   444.650   0       D411N   D411N   Wide   -
   11   446.550   0       D503N   D503N   Wide   -
   12   448.550   0       D606N   D606N   Wide   -
   13   452.425   0       D712N   D712N   Wide   -
   14   400.225   0          -       -    Wide   -
   15   435.650   0          -       -    Wide   -
   16   469.850   0          -       -    Wide   -
   23   445.500  -10         -       -    Wide   -

Current Channel: 1
Volume Level: 0
Transmit Power: High
Squelch Level: 3
VOX Level: 0
Squelch Tail Eliminate: On
Transmit Timer: 120
Busy Channel Lockout: Off
Scan Resume: Timeout
Alarm Timer: Off
Voice Prompt: English
Key Beep: On
Key Lock: Off
Battery Saver: On
Back Light: Key Press
FM Radio: America
FM Frequency: 65.0
Relay Mode: Off
VHF Range: 136.0-174.0
UHF Range: 400.0-470.0
)");
}

TEST(config, bf_f8hp_factory)
{
    auto result = show_config("bf-f8hp-factory.img");

    EXPECT_EQ(result, R"(Message: BAOFENGBF-F8HP

Channel Name    Receive  TxOffset R-Squel T-Squel Power FM     Scan BCL Scode PTTID
    1   -       140.1250  0          -       -    High  Wide   +    -   -     -
    2   -       145.2250  0          -       -    High  Wide   +    -   -     -
    3   -       150.3250  0          -       -    High  Wide   +    -   -     -
    4   -       440.1250  0          -       -    High  Wide   +    -   -     -
    5   -       445.2250  0          -       -    High  Wide   +    -   -     -
    6   -       450.3250  0          -       -    High  Wide   +    -   -     -

VFO Band Receive  TxOffset R-Squel T-Squel Step Power FM     Scode
 A  VHF  144.0000  0          -       -    20.0 High  Wide   -
 B  UHF  440.0000  0          -       -    20.0 High  Wide   -

Limit Lower Upper Enable
 VHF   136   174  +
 UHF   400   520  +

Squelch Level: 3
Battery Saver: 3
VOX Level: Off
Backlight Timeout: 5
Dual Watch: Off
Keypad Beep: On
TX Timer: 60
Voice Prompt: English
ANI Code: 80808
DTMF Sidetone: DTMF+ANI
Scan Resume: TO
Display Mode A: Frequency
Display Mode B: Frequency
Busy Channel Lockout: Off
Auto Key Lock: Off
Standby LED Color: Purple
RX LED Color: Blue
TX LED Color: Orange
Alarm Mode: Tone
Squelch Tail Eliminate: On
Squelch Tail Eliminate for Repeater: 5
Squelch Tail Repeater Delay: Off
Power-On Message: On
Roger Beep: Off
)");
}
