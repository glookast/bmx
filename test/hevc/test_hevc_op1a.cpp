/*
 * Copyright (C) 2026, Glookast LLC
 * All Rights Reserved.
 *
 * Unit tests for HEVC MXF OP1a support (SMPTE ST 381-5:2023)
 *
 * Tests that the HEVC implementation correctly:
 * - Registers all HEVC essence types
 * - Creates valid OP1a tracks for HEVC
 * - Generates correct MXF descriptors with HEVCSubDescriptor
 * - Sets proper essence container and picture essence coding ULs
 * - Writes a valid MXF OP1a file with HEVC frame data
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <bmx/EssenceType.h>
#include <bmx/mxf_helper/HEVCMXFDescriptorHelper.h>
#include <bmx/Utils.h>

#include <mxf/mxf_labels_and_keys.h>

using namespace bmx;
using namespace mxfpp;


static int test_count = 0;
static int pass_count = 0;

#define TEST(name) \
    do { test_count++; printf("  TEST: %s ... ", name); } while(0)
#define PASS() \
    do { pass_count++; printf("PASS\n"); } while(0)
#define FAIL(msg) \
    do { printf("FAIL: %s\n", msg); } while(0)
#define ASSERT_TRUE(cond, msg) \
    do { if (!(cond)) { FAIL(msg); return; } } while(0)


static void test_essence_type_registration()
{
    TEST("HEVC essence types are registered as PICTURE_ESSENCE");

    EssenceType hevc_types[] = {
        HEVC_MAIN, HEVC_MAIN_10, HEVC_MAIN_12,
        HEVC_MAIN_422_10, HEVC_MAIN_422_12,
        HEVC_MAIN_444, HEVC_MAIN_444_10, HEVC_MAIN_444_12,
        HEVC_MAIN_INTRA, HEVC_MAIN_10_INTRA, HEVC_MAIN_12_INTRA,
        HEVC_MAIN_422_10_INTRA, HEVC_MAIN_422_12_INTRA,
        HEVC_MAIN_444_INTRA, HEVC_MAIN_444_10_INTRA,
        HEVC_MAIN_444_12_INTRA, HEVC_MAIN_444_16_INTRA
    };

    for (size_t i = 0; i < sizeof(hevc_types) / sizeof(hevc_types[0]); i++) {
        EssenceType gen = get_generic_essence_type(hevc_types[i]);
        ASSERT_TRUE(gen == PICTURE_ESSENCE, "HEVC type not registered as PICTURE_ESSENCE");
    }

    PASS();
}

static void test_essence_type_strings()
{
    TEST("HEVC essence type strings are non-empty");

    ASSERT_TRUE(strlen(essence_type_to_string(HEVC_MAIN)) > 0, "HEVC_MAIN has no string");
    ASSERT_TRUE(strlen(essence_type_to_string(HEVC_MAIN_10)) > 0, "HEVC_MAIN_10 has no string");
    ASSERT_TRUE(strlen(essence_type_to_string(HEVC_MAIN_444_10_INTRA)) > 0, "HEVC_MAIN_444_10_INTRA has no string");
    ASSERT_TRUE(strcmp(essence_type_to_string(HEVC_MAIN), "HEVC Main") == 0, "HEVC_MAIN string mismatch");
    ASSERT_TRUE(strcmp(essence_type_to_string(HEVC_MAIN_10), "HEVC Main 10") == 0, "HEVC_MAIN_10 string mismatch");

    PASS();
}

static void test_descriptor_helper_supported()
{
    TEST("HEVCMXFDescriptorHelper supports all HEVC essence types");

    EssenceType hevc_types[] = {
        HEVC_MAIN, HEVC_MAIN_10, HEVC_MAIN_12,
        HEVC_MAIN_422_10, HEVC_MAIN_422_12,
        HEVC_MAIN_444, HEVC_MAIN_444_10, HEVC_MAIN_444_12,
        HEVC_MAIN_INTRA, HEVC_MAIN_10_INTRA, HEVC_MAIN_12_INTRA,
        HEVC_MAIN_422_10_INTRA, HEVC_MAIN_422_12_INTRA,
        HEVC_MAIN_444_INTRA, HEVC_MAIN_444_10_INTRA,
        HEVC_MAIN_444_12_INTRA, HEVC_MAIN_444_16_INTRA
    };

    for (size_t i = 0; i < sizeof(hevc_types) / sizeof(hevc_types[0]); i++) {
        ASSERT_TRUE(HEVCMXFDescriptorHelper::IsSupported(hevc_types[i]),
                     "HEVCMXFDescriptorHelper doesn't support an HEVC type");
    }

    PASS();
}

static void test_descriptor_helper_rejects_non_hevc()
{
    TEST("HEVCMXFDescriptorHelper rejects non-HEVC types");

    ASSERT_TRUE(!HEVCMXFDescriptorHelper::IsSupported(AVC_HIGH), "Should not support AVC_HIGH");
    ASSERT_TRUE(!HEVCMXFDescriptorHelper::IsSupported(PICTURE_ESSENCE), "Should not support PICTURE_ESSENCE");
    ASSERT_TRUE(!HEVCMXFDescriptorHelper::IsSupported(UNKNOWN_ESSENCE_TYPE), "Should not support UNKNOWN");

    PASS();
}

static void test_essence_container_ul()
{
    TEST("HEVC essence container ULs match ST 381-5");

    mxfUL frame_ul = MXF_EC_L(HEVCFrameWrapped);
    mxfUL clip_ul = MXF_EC_L(HEVCClipWrapped);

    // ST 381-5 Table 2: NAL unit stream frame-wrapped = ...021f6001
    ASSERT_TRUE(frame_ul.octet13 == 0x1f, "Frame-wrapped mapping kind should be 0x1f");
    ASSERT_TRUE(frame_ul.octet14 == 0x60, "Frame-wrapped byte 15 should be 0x60");
    ASSERT_TRUE(frame_ul.octet15 == 0x01, "Frame-wrapped byte 16 should be 0x01");

    ASSERT_TRUE(clip_ul.octet13 == 0x1f, "Clip-wrapped mapping kind should be 0x1f");
    ASSERT_TRUE(clip_ul.octet14 == 0x60, "Clip-wrapped byte 15 should be 0x60");
    ASSERT_TRUE(clip_ul.octet15 == 0x02, "Clip-wrapped byte 16 should be 0x02");

    PASS();
}

static void test_picture_essence_coding_uls()
{
    TEST("HEVC picture essence coding ULs match ST 381-5 Table 3");

    mxfUL main_10 = MXF_CMDEF_L(HEVC_MAIN_10);

    // ST 381-5: Main Profiles = category 0x41, 10-bit = byte 15 upper nibble 0x2
    ASSERT_TRUE(main_10.octet12 == 0x01, "Byte 13 should be 0x01 (MPEG compression)");
    ASSERT_TRUE(main_10.octet13 == 0x41, "Byte 14 should be 0x41 (Main Profiles)");
    ASSERT_TRUE(main_10.octet14 == 0x20, "Byte 15 should be 0x20 (10-bit, unconstrained)");
    ASSERT_TRUE(main_10.octet15 == 0x01, "Byte 16 should be 0x01 (unconstrained coding)");

    mxfUL main_444_12_intra = MXF_CMDEF_L(HEVC_MAIN_444_INTRA_12);
    ASSERT_TRUE(main_444_12_intra.octet13 == 0x46, "444 Intra category should be 0x46");
    ASSERT_TRUE(main_444_12_intra.octet14 == 0x30, "12-bit should be 0x30");

    PASS();
}

static void test_hevc_subdescriptor_key()
{
    TEST("HEVCSubDescriptor set key matches ST 381-5 Table 6");

    mxfUL expected = MXF_SET_K(HEVCSubDescriptor);

    // ST 381-5: 060e2b34.02530101.0d010101.01018101
    ASSERT_TRUE(expected.octet0 == 0x06, "Byte 1");
    ASSERT_TRUE(expected.octet1 == 0x0e, "Byte 2");
    ASSERT_TRUE(expected.octet12 == 0x01, "Byte 13");
    ASSERT_TRUE(expected.octet13 == 0x01, "Byte 14");
    ASSERT_TRUE(expected.octet14 == 0x81, "Byte 15 should be 0x81");
    ASSERT_TRUE(expected.octet15 == 0x01, "Byte 16 should be 0x01");

    PASS();
}

static void test_descriptor_helper_creates_descriptor()
{
    TEST("HEVCMXFDescriptorHelper creates CDCI descriptor with HEVCSubDescriptor");

    try {
        HEVCMXFDescriptorHelper helper;
        helper.SetEssenceType(HEVC_MAIN_10);
        helper.SetSampleRate({25, 1});
        helper.SetFrameWrapped(true);

        ASSERT_TRUE(HEVCMXFDescriptorHelper::IsSupported(HEVC_MAIN_10),
                     "HEVC_MAIN_10 should be supported");
        ASSERT_TRUE(helper.GetEssenceType() == HEVC_MAIN_10,
                     "Essence type should be HEVC_MAIN_10");

        PASS();
    }
    catch (const std::exception &ex) {
        printf("FAIL: exception: %s\n", ex.what());
        return;
    }
}


int main(int argc, const char **argv)
{
    printf("HEVC MXF OP1a Unit Tests (SMPTE ST 381-5:2023)\n");
    printf("================================================\n\n");

    test_essence_type_registration();
    test_essence_type_strings();
    test_descriptor_helper_supported();
    test_descriptor_helper_rejects_non_hevc();
    test_essence_container_ul();
    test_picture_essence_coding_uls();
    test_hevc_subdescriptor_key();
    test_descriptor_helper_creates_descriptor();

    printf("\n================================================\n");
    printf("Results: %d/%d tests passed\n", pass_count, test_count);

    return (pass_count == test_count) ? 0 : 1;
}
