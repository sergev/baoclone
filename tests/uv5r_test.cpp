#include <cstdio>
#include <fstream>
#include <stdio.h>

#include "util.h"
#include "radio.h"

TEST(uv5r, vfo)
{
    //
    // Test #1: check frequency 144.950
    //
    int vfo_index = 0;
    int band = 'U';
    double freq_mhz = strtod("144.950", NULL);
    int lowpower = 0;
    int wide = 1;
    int step = 6;
    int scode = 0;
    uv5r_setup_vfo(vfo_index, band, freq_mhz, 0.0, 0, 0, step, lowpower, wide, scode);

    int result_band = -1, result_hz = -1, result_offset = -1, result_rx_ctcs = -1;
    int result_tx_ctcs = -1, result_rx_dcs = -1, result_tx_dcs = -1;
    int result_lowpower = -1, result_wide = -1, result_step = -1, result_scode = -1;
    uv5r_decode_vfo(vfo_index, &result_band, &result_hz, &result_offset,
                    &result_rx_ctcs, &result_tx_ctcs, &result_rx_dcs, &result_tx_dcs,
                    &result_lowpower, &result_wide, &result_step, &result_scode);

    EXPECT_EQ(result_band, 1);
    EXPECT_EQ(result_hz, 144'950'000);
    EXPECT_EQ(result_offset, 0);
    EXPECT_EQ(result_rx_ctcs, 0);
    EXPECT_EQ(result_tx_ctcs, 0);
    EXPECT_EQ(result_rx_dcs, 0);
    EXPECT_EQ(result_tx_dcs, 0);
    EXPECT_EQ(result_lowpower, 0);
    EXPECT_EQ(result_wide, 1);
    EXPECT_EQ(result_step, 6);
    EXPECT_EQ(result_scode, 0);

    //
    // Test #2: check frequency 144.951
    //
    freq_mhz = strtod("144.951", NULL);
    uv5r_setup_vfo(vfo_index, band, freq_mhz, 0.0, 0, 0, step, lowpower, wide, scode);
    uv5r_decode_vfo(vfo_index, &result_band, &result_hz, &result_offset,
                    &result_rx_ctcs, &result_tx_ctcs, &result_rx_dcs, &result_tx_dcs,
                    &result_lowpower, &result_wide, &result_step, &result_scode);
    EXPECT_EQ(result_hz, 144'951'000);

    //
    // Test #3: check frequency 144.900
    //
    freq_mhz = strtod("144.900", NULL);
    uv5r_setup_vfo(vfo_index, band, freq_mhz, 0.0, 0, 0, step, lowpower, wide, scode);
    uv5r_decode_vfo(vfo_index, &result_band, &result_hz, &result_offset,
                    &result_rx_ctcs, &result_tx_ctcs, &result_rx_dcs, &result_tx_dcs,
                    &result_lowpower, &result_wide, &result_step, &result_scode);
    EXPECT_EQ(result_hz, 144'900'000);
}
