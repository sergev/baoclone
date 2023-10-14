#include <cstdio>
#include <fstream>
#include <stdio.h>

#include "util.h"
#include "radio.h"

//
// Show configuration from image file, located in the examples directory.
//
static std::string show_version(const std::string &img_basename)
{
    std::string base_name       = get_test_name();
    std::string output_filename = base_name + ".version";
    std::string img_filename    = std::string(TEST_DIR "/../examples/") + img_basename;

    FILE *output = fopen(output_filename.c_str(), "w");
    EXPECT_NE(output, nullptr);

    radio_read_image(img_filename.c_str());
    radio_print_version(output, 1);
    fclose(output);
    return file_contents(output_filename);
}

TEST(version, uv_5r_factory)
{
    auto result = show_version("uv-5r-factory.img");

    EXPECT_EQ(result, R"(Radio: Baofeng UV-5R
Firmware: Ver  BFB291
Serial: CCCCCCCDDDDDDD
)");
}

TEST(version, bf_888s_factory)
{
    auto result = show_version("bf-888s-factory.img");

    EXPECT_EQ(result, "Radio: Baofeng BF-888S\n");
}

TEST(version, bf_888s_v2_factory)
{
    auto result = show_version("bf-888s-factory2.img");

    EXPECT_EQ(result, "Radio: Baofeng BF-888S\n");
}

TEST(version, uv_b5_factory)
{
    auto result = show_version("uv-b5-factory.img");

    EXPECT_EQ(result, "Radio: Baofeng UV-B5\n");
}

TEST(version, bf_t1_factory)
{
    auto result = show_version("bf-t1-factory.img");

    EXPECT_EQ(result, "Radio: Baofeng BF-T1\n");
}

TEST(version, bf_f8hp_factory)
{
    auto result = show_version("bf-f8hp-factory.img");

    EXPECT_EQ(result, R"(Radio: Baofeng UV-5R
Firmware: N5R340BF8HP-1
Serial: 151123H
)");
}
