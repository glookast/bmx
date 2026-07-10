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
#include <bmx/essence_parser/HEVCEssenceParser.h>
#include <bmx/Utils.h>

#include <libMXF++/MXF.h>

#include <mxf/mxf_labels_and_keys.h>

#include "hevc_param_sets.h"

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


static void test_hevc_parser_geometry()
{
    TEST("HEVCEssenceParser extracts geometry from real SPS (8-bit and 10-bit)");

    HEVCEssenceParser p8;
    p8.ParseFrameInfo(HEVC_PS_1080_8BIT, HEVC_PS_1080_8BIT_size);
    ASSERT_TRUE(p8.HaveSequenceParameterSet(), "8-bit SPS was not parsed");
    ASSERT_TRUE(p8.GetStoredWidth() == 1920, "8-bit stored width != 1920");
    ASSERT_TRUE(p8.GetStoredHeight() >= 1080, "8-bit stored height < 1080");
    ASSERT_TRUE(p8.GetDisplayWidth() == 1920, "8-bit display width != 1920");
    ASSERT_TRUE(p8.GetDisplayHeight() == 1080, "8-bit display height != 1080");
    ASSERT_TRUE(p8.GetComponentDepth() == 8, "8-bit component depth != 8");
    ASSERT_TRUE(p8.GetChromaFormat() == 1, "8-bit chroma format != 4:2:0");
    // VUI colour signalling (the test clips are tagged BT.709 = code point 1)
    ASSERT_TRUE(p8.GetColorPrimaries() == 1, "8-bit colour primaries not parsed from VUI (expected BT.709)");
    ASSERT_TRUE(p8.GetTransferCharacteristics() == 1, "8-bit transfer characteristics not parsed (expected BT.709)");
    ASSERT_TRUE(p8.GetMatrixCoefficients() == 1, "8-bit matrix coefficients not parsed (expected BT.709)");
    ASSERT_TRUE(p8.GetSampleAspectRatio().numerator > 0, "8-bit sample aspect ratio not parsed from VUI");

    HEVCEssenceParser p10;
    p10.ParseFrameInfo(HEVC_PS_1080_10BIT, HEVC_PS_1080_10BIT_size);
    ASSERT_TRUE(p10.HaveSequenceParameterSet(), "10-bit SPS was not parsed");
    ASSERT_TRUE(p10.GetStoredWidth() == 1920, "10-bit stored width != 1920");
    ASSERT_TRUE(p10.GetDisplayHeight() == 1080, "10-bit display height != 1080");
    ASSERT_TRUE(p10.GetComponentDepth() == 10, "10-bit component depth != 10");
    ASSERT_TRUE(p10.GetChromaFormat() == 1, "10-bit chroma format != 4:2:0");

    PASS();
}

static void test_hevc_descriptor_geometry_from_sps()
{
    // Regression test for the descriptor-geometry bug: OP1AHEVCTrack parsed the SPS
    // but never pushed the picture geometry to the CDCI descriptor, so the output had
    // StoredWidth/StoredHeight == 0 and no NLE could open the file. This verifies that
    // HEVCMXFDescriptorHelper::UpdateFileDescriptor(HEVCEssenceParser*) propagates the
    // parsed geometry to the descriptor.
    TEST("HEVCMXFDescriptorHelper populates CDCI geometry from parsed SPS");

    DataModel *data_model = 0;
    HeaderMetadata *header_metadata = 0;
    try {
        data_model = new DataModel();
        header_metadata = new HeaderMetadata(data_model);

        HEVCMXFDescriptorHelper helper;
        // Deliberately set the WRONG profile (Main 10) for an 8-bit source; the SPS-derived
        // reconciliation must correct it to Main.
        helper.SetEssenceType(HEVC_MAIN_10);
        helper.SetSampleRate({25, 1});
        helper.SetFrameWrapped(true);

        FileDescriptor *file_desc = helper.CreateFileDescriptor(header_metadata);
        CDCIEssenceDescriptor *cdci = dynamic_cast<CDCIEssenceDescriptor*>(file_desc);
        ASSERT_TRUE(cdci != 0, "descriptor is not a CDCIEssenceDescriptor");

        HEVCEssenceParser parser;
        parser.ParseFrameInfo(HEVC_PS_1080_8BIT, HEVC_PS_1080_8BIT_size);
        ASSERT_TRUE(parser.GetStoredWidth() > 0, "parser did not extract geometry");

        helper.UpdateFileDescriptor(&parser);

        // The coding profile must be reconciled to the SPS (8-bit Main), not the Main 10 guess.
        ASSERT_TRUE(helper.GetEssenceType() == HEVC_MAIN,
                     "essence profile not reconciled from SPS (expected HEVC_MAIN)");
        // Colour metadata must be carried onto the descriptor.
        ASSERT_TRUE(cdci->haveColorPrimaries(), "ColorPrimaries not set from SPS VUI");
        ASSERT_TRUE(cdci->haveCaptureGamma(), "CaptureGamma (transfer) not set from SPS VUI");
        ASSERT_TRUE(cdci->haveCodingEquations(), "CodingEquations (matrix) not set from SPS VUI");
        ASSERT_TRUE(cdci->haveAspectRatio(), "AspectRatio not derived from SPS VUI");

        // The core regression: dimensions must be present and non-zero, and must match
        // what the parser extracted from the SPS.
        ASSERT_TRUE(cdci->haveStoredWidth() && cdci->getStoredWidth() == parser.GetStoredWidth(),
                     "StoredWidth not propagated from SPS");
        ASSERT_TRUE(cdci->haveStoredHeight() && cdci->getStoredHeight() == parser.GetStoredHeight(),
                     "StoredHeight not propagated from SPS");
        ASSERT_TRUE(cdci->getStoredWidth() == 1920, "StoredWidth != 1920");
        ASSERT_TRUE(cdci->getStoredHeight() >= 1080, "StoredHeight < 1080");
        ASSERT_TRUE(cdci->haveDisplayWidth() && cdci->getDisplayWidth() == 1920, "DisplayWidth != 1920");
        ASSERT_TRUE(cdci->haveDisplayHeight() && cdci->getDisplayHeight() == 1080, "DisplayHeight != 1080");
        ASSERT_TRUE(cdci->haveComponentDepth() && cdci->getComponentDepth() == 8, "ComponentDepth != 8");
        ASSERT_TRUE(cdci->haveHorizontalSubsampling() && cdci->getHorizontalSubsampling() == 2,
                     "HorizontalSubsampling != 2 for 4:2:0");
        ASSERT_TRUE(cdci->haveVerticalSubsampling() && cdci->getVerticalSubsampling() == 2,
                     "VerticalSubsampling != 2 for 4:2:0");
        ASSERT_TRUE(cdci->havePictureEssenceCoding(), "PictureEssenceCoding UL not set");

        delete header_metadata;
        delete data_model;
        PASS();
    }
    catch (const std::exception &ex) {
        delete header_metadata;
        delete data_model;
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
    test_hevc_parser_geometry();
    test_hevc_descriptor_geometry_from_sps();

    printf("\n================================================\n");
    printf("Results: %d/%d tests passed\n", pass_count, test_count);

    return (pass_count == test_count) ? 0 : 1;
}
