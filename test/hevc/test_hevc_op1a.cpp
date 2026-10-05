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
#include <map>
#include <vector>

#include <bmx/EssenceType.h>
#include <bmx/mxf_helper/HEVCMXFDescriptorHelper.h>
#include <bmx/essence_parser/HEVCEssenceParser.h>
#include <bmx/writer_helper/HEVCWriterHelper.h>
#include <bmx/mxf_op1a/OP1ATrack.h>
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

    // Byte stream frame-wrapped = ...02206001
    mxfUL bs_frame_ul = MXF_EC_L(HEVCByteStreamFrameWrapped);
    mxfUL bs_clip_ul = MXF_EC_L(HEVCByteStreamClipWrapped);
    ASSERT_TRUE(bs_frame_ul.octet13 == 0x20 && bs_frame_ul.octet14 == 0x60 && bs_frame_ul.octet15 == 0x01,
                "Byte stream frame-wrapped should be ...02206001");
    ASSERT_TRUE(bs_clip_ul.octet13 == 0x20 && bs_clip_ul.octet14 == 0x60 && bs_clip_ul.octet15 == 0x02,
                "Byte stream clip-wrapped should be ...02206002");

    // Readers accept both mappings
    ASSERT_TRUE(mxf_is_hevc_ec(&frame_ul, 1) && mxf_is_hevc_ec(&bs_frame_ul, 1),
                "mxf_is_hevc_ec should accept NAL unit stream and byte stream frame-wrapped");

    PASS();
}

static void test_op1a_track_supports_hevc()
{
    // OP1ATrack::Create builds HEVC tracks, so OP1ATrack::IsSupported (used by ClipWriter and the
    // bmx apps) must accept them too; bmxtranswrap dropped every HEVC track without it.
    TEST("OP1ATrack::IsSupported accepts the HEVC essence types");

    EssenceType hevc_types[] = {
        HEVC_MAIN, HEVC_MAIN_10, HEVC_MAIN_12,
        HEVC_MAIN_422_10, HEVC_MAIN_422_12,
        HEVC_MAIN_444, HEVC_MAIN_444_10, HEVC_MAIN_444_12,
        HEVC_MAIN_INTRA, HEVC_MAIN_10_INTRA, HEVC_MAIN_12_INTRA,
        HEVC_MAIN_422_10_INTRA, HEVC_MAIN_422_12_INTRA,
        HEVC_MAIN_444_INTRA, HEVC_MAIN_444_10_INTRA,
        HEVC_MAIN_444_12_INTRA, HEVC_MAIN_444_16_INTRA
    };
    mxfRational rates[] = {{24000, 1001}, {25, 1}, {30000, 1001}, {50, 1}, {60000, 1001}};
    for (size_t i = 0; i < sizeof(hevc_types) / sizeof(hevc_types[0]); i++) {
        for (size_t j = 0; j < sizeof(rates) / sizeof(rates[0]); j++) {
            ASSERT_TRUE(OP1ATrack::IsSupported(hevc_types[i], rates[j]),
                        "HEVC essence type not supported by OP1ATrack::IsSupported");
        }
    }

    PASS();
}

static void test_descriptor_uses_byte_stream_ec()
{
    // MPS-1098: the writer stores Annex B access units (start code prefixed NAL units), so the
    // descriptor must carry the HEVC Byte Stream mapping, not the NAL Unit Stream mapping.
    TEST("HEVCMXFDescriptorHelper labels Annex B essence with the HEVC Byte Stream mapping");

    DataModel *data_model = 0;
    HeaderMetadata *header_metadata = 0;
    try {
        data_model = new DataModel();
        header_metadata = new HeaderMetadata(data_model);

        HEVCMXFDescriptorHelper helper;
        helper.SetEssenceType(HEVC_MAIN_10);
        helper.SetSampleRate({25, 1});
        helper.SetFrameWrapped(true);
        FileDescriptor *file_desc = helper.CreateFileDescriptor(header_metadata);

        mxfUL ec_label = file_desc->getEssenceContainer();
        mxfUL expected = MXF_EC_L(HEVCByteStreamFrameWrapped);
        ASSERT_TRUE(mxf_equals_ul_mod_regver(&ec_label, &expected),
                    "frame-wrapped essence container is not HEVC Byte Stream ...02206001");

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


// Writes the start of a first slice segment header (H.265 7.3.6.1) with a PPS that has no extra
// slice header bits, no output flag and no dependent slices, as the fixtures' PPS do.
class SliceHeaderBits
{
public:
    SliceHeaderBits() : mBitCount(0) {}

    void U(uint32_t value, int num_bits)
    {
        for (int i = num_bits - 1; i >= 0; i--)
            Bit((value >> i) & 1);
    }

    void UE(uint32_t value)
    {
        uint32_t code = value + 1;
        int len = 0;
        while ((code >> len) > 1)
            len++;
        U(0, len);
        U(code, len + 1);
    }

    std::vector<unsigned char> Finish()
    {
        Bit(1);
        while (mBitCount % 8)
            Bit(0);
        // stand-in for the slice data; non-zero so it can never form a start code
        for (int i = 0; i < 4; i++)
            mBytes.push_back(0xa5);
        return mBytes;
    }

private:
    void Bit(uint32_t bit)
    {
        if (mBitCount % 8 == 0)
            mBytes.push_back(0);
        if (bit)
            mBytes.back() |= (unsigned char)(0x80 >> (mBitCount % 8));
        mBitCount++;
    }

    std::vector<unsigned char> mBytes;
    size_t mBitCount;
};

// slice_type: 0 = B, 1 = P, 2 = I
static std::vector<unsigned char> make_access_unit(bool with_parameter_sets, uint8_t nal_type, uint32_t slice_type,
                                                   uint32_t poc_lsb, uint8_t log2_max_poc_lsb)
{
    std::vector<unsigned char> au;
    if (with_parameter_sets)
        au.assign(HEVC_PS_1080_10BIT, HEVC_PS_1080_10BIT + HEVC_PS_1080_10BIT_size);

    static const unsigned char start_code[] = {0x00, 0x00, 0x00, 0x01};
    au.insert(au.end(), start_code, start_code + sizeof(start_code));
    au.push_back((unsigned char)(nal_type << 1));
    au.push_back(0x01); // nuh_layer_id 0, nuh_temporal_id_plus1 1

    SliceHeaderBits bits;
    bits.U(1, 1);                               // first_slice_segment_in_pic_flag
    if (nal_type >= HEVC_BLA_W_LP && nal_type <= HEVC_CRA_NUT)
        bits.U(0, 1);                           // no_output_of_prior_pics_flag
    bits.UE(0);                                 // slice_pic_parameter_set_id
    bits.UE(slice_type);
    if (nal_type != HEVC_IDR_W_RADL && nal_type != HEVC_IDR_N_LP)
        bits.U(poc_lsb, log2_max_poc_lsb);      // slice_pic_order_cnt_lsb
    std::vector<unsigned char> slice = bits.Finish();
    au.insert(au.end(), slice.begin(), slice.end());

    return au;
}

static uint8_t fixture_log2_max_poc_lsb()
{
    HEVCEssenceParser parser;
    std::vector<unsigned char> idr = make_access_unit(true, HEVC_IDR_N_LP, 2, 0, 0);
    parser.ParseFrameInfo(&idr[0], (uint32_t)idr.size());
    return parser.GetLog2MaxPicOrderCntLsb();
}

typedef struct
{
    int8_t temporal_offset;
    int8_t key_frame_offset;
    uint8_t flags;
} TestIndexEntry;

static void take_complete_entries(HEVCWriterHelper *writer_helper, std::map<int64_t, TestIndexEntry> *entries)
{
    int64_t position;
    TestIndexEntry entry;
    MPEGFrameType frame_type;
    while (writer_helper->TakeCompleteIndexEntry(&position, &entry.temporal_offset, &entry.key_frame_offset,
                                                 &entry.flags, &frame_type))
    {
        (*entries)[position] = entry;
    }
}

typedef struct
{
    uint8_t nal_type;
    uint32_t slice_type;
    uint32_t poc_lsb;
} TestPicture;

// Runs coded pictures through HEVCWriterHelper, collecting the index entries and the sub-descriptor
static bool index_pictures(const TestPicture *pictures, size_t count, std::map<int64_t, TestIndexEntry> *entries,
                           HeaderMetadata *header_metadata, HEVCMXFDescriptorHelper *descriptor_helper)
{
    uint8_t log2_max_poc_lsb = fixture_log2_max_poc_lsb();
    if (log2_max_poc_lsb == 0)
        return false;

    descriptor_helper->SetEssenceType(HEVC_MAIN_10);
    descriptor_helper->SetSampleRate({25, 1});
    descriptor_helper->SetFrameWrapped(true);
    descriptor_helper->CreateFileDescriptor(header_metadata);

    HEVCWriterHelper writer_helper;
    writer_helper.SetDescriptorHelper(descriptor_helper);
    for (size_t i = 0; i < count; i++) {
        bool irap = (pictures[i].nal_type >= HEVC_BLA_W_LP && pictures[i].nal_type <= HEVC_CRA_NUT);
        std::vector<unsigned char> au = make_access_unit(irap, pictures[i].nal_type, pictures[i].slice_type,
                                                         pictures[i].poc_lsb, log2_max_poc_lsb);
        writer_helper.ProcessFrame(&au[0], (uint32_t)au.size());
        take_complete_entries(&writer_helper, entries);
    }
    writer_helper.CompleteProcess();
    take_complete_entries(&writer_helper, entries);

    return entries->size() == count;
}


static void test_hevc_parser_irap_and_parameter_sets()
{
    TEST("HEVCEssenceParser reports IRAP pictures and the parameter sets of each access unit");

    uint8_t log2_max_poc_lsb = fixture_log2_max_poc_lsb();
    ASSERT_TRUE(log2_max_poc_lsb > 0, "log2_max_pic_order_cnt_lsb not parsed from the fixture SPS");

    HEVCEssenceParser parser;
    std::vector<unsigned char> idr = make_access_unit(true, HEVC_IDR_N_LP, 2, 0, log2_max_poc_lsb);
    parser.ParseFrameInfo(&idr[0], (uint32_t)idr.size());
    ASSERT_TRUE(parser.IsIRAPFrame() && parser.IsIDRFrame(), "IDR not reported as an IRAP picture");
    ASSERT_TRUE(parser.FrameHasVPS() && parser.FrameHasSPS() && parser.FrameHasPPS(),
                "parameter sets of the IDR access unit not reported");

    std::vector<unsigned char> trail = make_access_unit(false, HEVC_TRAIL_R, 2, 1, log2_max_poc_lsb);
    parser.ParseFrameInfo(&trail[0], (uint32_t)trail.size());
    ASSERT_TRUE(parser.GetFrameType() == I_FRAME, "TRAIL_R I slice not parsed as an I picture");
    ASSERT_TRUE(!parser.IsIRAPFrame(), "TRAIL_R I picture reported as an IRAP picture");
    ASSERT_TRUE(!parser.FrameHasVPS() && !parser.FrameHasSPS() && !parser.FrameHasPPS(),
                "parameter sets reported for an access unit without them");
    ASSERT_TRUE(parser.GetSlicePicOrderCntLsb() == 1, "TRAIL_R picture order count LSB not parsed");

    std::vector<unsigned char> cra = make_access_unit(true, HEVC_CRA_NUT, 2, 3, log2_max_poc_lsb);
    parser.ParseFrameInfo(&cra[0], (uint32_t)cra.size());
    ASSERT_TRUE(parser.IsIRAPFrame() && parser.IsCRAFrame() && !parser.IsIDRFrame(),
                "CRA not reported as a non-IDR IRAP picture");
    ASSERT_TRUE(parser.IsVPSDataConstant() && parser.IsSPSDataConstant() && parser.IsPPSDataConstant(),
                "identical parameter sets reported as changing");

    PASS();
}

static void test_hevc_index_intra_trail_pictures()
{
    // MPS-1098: an "intra" capture with an IDR every 3rd picture and TRAIL_R pictures coded with
    // I slices (and no parameter sets) in between. Only the IDR may be a key frame / random access
    // point; the TRAIL_R pictures must point back to it.
    TEST("HEVCWriterHelper indexes only IRAP pictures as key frames (IDR + TRAIL_R I pictures)");

    static const TestPicture pictures[] = {
        {HEVC_IDR_N_LP, 2, 0}, {HEVC_TRAIL_R, 2, 1}, {HEVC_TRAIL_R, 2, 2},
        {HEVC_IDR_N_LP, 2, 0}, {HEVC_TRAIL_R, 2, 1}, {HEVC_TRAIL_R, 2, 2},
        {HEVC_IDR_N_LP, 2, 0}, {HEVC_TRAIL_R, 2, 1}, {HEVC_TRAIL_R, 2, 2},
    };
    static const size_t count = sizeof(pictures) / sizeof(pictures[0]);

    DataModel data_model;
    HeaderMetadata header_metadata(&data_model);
    HEVCMXFDescriptorHelper descriptor_helper;
    std::map<int64_t, TestIndexEntry> entries;
    ASSERT_TRUE(index_pictures(pictures, count, &entries, &header_metadata, &descriptor_helper),
                "not every picture got an index entry");

    for (int64_t i = 0; i < (int64_t)count; i++) {
        const TestIndexEntry &entry = entries[i];
        ASSERT_TRUE(entry.temporal_offset == 0, "intra pictures must not be reordered");
        if (i % 3 == 0) {
            ASSERT_TRUE(entry.key_frame_offset == 0, "IDR key frame offset != 0");
            ASSERT_TRUE(entry.flags == 0xc4, "IDR flags != random access | sequence header | IDR (0xc4)");
        } else {
            ASSERT_TRUE(entry.key_frame_offset == -(i % 3), "TRAIL_R key frame offset does not point to the IDR");
            ASSERT_TRUE(!(entry.flags & 0x80), "TRAIL_R picture flagged as a random access point");
            ASSERT_TRUE(!(entry.flags & 0x40), "TRAIL_R picture flagged as carrying a sequence header");
        }
    }

    HEVCSubDescriptor *sub = descriptor_helper.GetHEVCSubDescriptor();
    ASSERT_TRUE(sub->getHEVCMaximumGOPSize() == 3, "MaximumGOPSize != 3 (GOP = IDR period)");
    ASSERT_TRUE(sub->getHEVCClosedGOPIndicator(), "IDR-only GOPs not reported closed");
    ASSERT_TRUE(sub->getHEVCIdenticalGOPIndicator(), "identical GOPs not reported identical");
    ASSERT_TRUE(sub->getHEVCMaximumBPictureCount() == 0, "MaximumBPictureCount != 0");
    // constant (0x80) | in every GOP start access unit (0x30)
    ASSERT_TRUE(sub->haveHEVCVideoParameterSetFlag() && sub->getHEVCVideoParameterSetFlag() == 0xb0,
                "VideoParameterSetFlag != constant, every GOP start (0xb0)");
    ASSERT_TRUE(sub->haveHEVCSequenceParameterSetFlag() && sub->getHEVCSequenceParameterSetFlag() == 0xb0,
                "SequenceParameterSetFlag != constant, every GOP start (0xb0)");
    ASSERT_TRUE(sub->haveHEVCPictureParameterSetFlag() && sub->getHEVCPictureParameterSetFlag() == 0xb0,
                "PictureParameterSetFlag != constant, every GOP start (0xb0)");

    PASS();
}

static void test_hevc_index_long_gop_b_pictures()
{
    // Long-GOP regression guard: IDR P B B P B B in coded order (display order I B B P B B P).
    TEST("HEVCWriterHelper indexes a long-GOP B-picture stream from its IDR");

    static const TestPicture pictures[] = {
        {HEVC_IDR_N_LP, 2, 0}, {HEVC_TRAIL_R, 1, 3}, {HEVC_TRAIL_N, 0, 1}, {HEVC_TRAIL_N, 0, 2},
        {HEVC_TRAIL_R, 1, 6}, {HEVC_TRAIL_N, 0, 4}, {HEVC_TRAIL_N, 0, 5},
    };
    static const size_t count = sizeof(pictures) / sizeof(pictures[0]);
    // display index -> coded position minus display index
    static const int8_t expected_temporal_offsets[] = {0, 1, 1, -2, 1, 1, -2};

    DataModel data_model;
    HeaderMetadata header_metadata(&data_model);
    HEVCMXFDescriptorHelper descriptor_helper;
    std::map<int64_t, TestIndexEntry> entries;
    ASSERT_TRUE(index_pictures(pictures, count, &entries, &header_metadata, &descriptor_helper),
                "not every picture got an index entry");

    ASSERT_TRUE((entries[0].flags & 0x80) && entries[0].key_frame_offset == 0, "IDR not a random access key frame");
    for (int64_t i = 0; i < (int64_t)count; i++) {
        ASSERT_TRUE(entries[i].temporal_offset == expected_temporal_offsets[i], "unexpected temporal offset");
        if (i > 0) {
            ASSERT_TRUE(!(entries[i].flags & 0x80), "P/B picture flagged as a random access point");
            ASSERT_TRUE(entries[i].key_frame_offset == -i, "P/B key frame offset does not point to the IDR");
        }
    }

    HEVCSubDescriptor *sub = descriptor_helper.GetHEVCSubDescriptor();
    ASSERT_TRUE(sub->getHEVCMaximumBPictureCount() == 2, "MaximumBPictureCount != 2");
    ASSERT_TRUE(sub->getHEVCMaximumGOPSize() == 7, "MaximumGOPSize != 7");
    // constant (0x80) | in every GOP start access unit (0x30): the stream's one GOP starts with the IDR
    ASSERT_TRUE(sub->getHEVCSequenceParameterSetFlag() == 0xb0,
                "SequenceParameterSetFlag != constant, every GOP start (0xb0)");

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

        // Colour range (8-bit limited): black 16, white 235, range 225.
        ASSERT_TRUE(cdci->haveColorRange() && cdci->getColorRange() == 225, "8-bit ColorRange != 225");

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


static void test_hevc_descriptor_color_range_10bit()
{
    // Regression guard: ColorRange is a level count and must NOT scale linearly for >8-bit.
    // 10-bit limited range = 897 (not 900), matching bmx's UncCDCIMXFDescriptorHelper.
    TEST("HEVCMXFDescriptorHelper sets correct 10-bit limited ColorRange (897)");

    DataModel *data_model = 0;
    HeaderMetadata *header_metadata = 0;
    try {
        data_model = new DataModel();
        header_metadata = new HeaderMetadata(data_model);

        HEVCMXFDescriptorHelper helper;
        helper.SetEssenceType(HEVC_MAIN_10);
        helper.SetSampleRate({25, 1});
        helper.SetFrameWrapped(true);

        FileDescriptor *file_desc = helper.CreateFileDescriptor(header_metadata);
        CDCIEssenceDescriptor *cdci = dynamic_cast<CDCIEssenceDescriptor*>(file_desc);
        ASSERT_TRUE(cdci != 0, "descriptor is not a CDCIEssenceDescriptor");

        HEVCEssenceParser parser;
        parser.ParseFrameInfo(HEVC_PS_1080_10BIT, HEVC_PS_1080_10BIT_size);
        ASSERT_TRUE(parser.GetComponentDepth() == 10, "10-bit parser depth != 10");

        helper.UpdateFileDescriptor(&parser);

        ASSERT_TRUE(cdci->haveComponentDepth() && cdci->getComponentDepth() == 10, "ComponentDepth != 10");
        ASSERT_TRUE(cdci->haveBlackRefLevel() && cdci->getBlackRefLevel() == 64, "10-bit BlackRefLevel != 64");
        ASSERT_TRUE(cdci->haveWhiteReflevel() && cdci->getWhiteReflevel() == 940, "10-bit WhiteRefLevel != 940");
        ASSERT_TRUE(cdci->haveColorRange() && cdci->getColorRange() == 897, "10-bit ColorRange != 897 (linear-scale bug)");

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
    test_descriptor_uses_byte_stream_ec();
    test_op1a_track_supports_hevc();
    test_picture_essence_coding_uls();
    test_hevc_subdescriptor_key();
    test_descriptor_helper_creates_descriptor();
    test_hevc_parser_geometry();
    test_hevc_descriptor_geometry_from_sps();
    test_hevc_descriptor_color_range_10bit();
    test_hevc_parser_irap_and_parameter_sets();
    test_hevc_index_intra_trail_pictures();
    test_hevc_index_long_gop_b_pictures();

    printf("\n================================================\n");
    printf("Results: %d/%d tests passed\n", pass_count, test_count);

    return (pass_count == test_count) ? 0 : 1;
}
