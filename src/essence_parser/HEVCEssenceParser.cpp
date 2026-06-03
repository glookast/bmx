/*
 * Copyright (C) 2026, Glookast LLC
 * All Rights Reserved.
 *
 * HEVC/H.265 Essence Parser
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <cstdlib>

#include <bmx/essence_parser/HEVCEssenceParser.h>
#include <bmx/BMXException.h>
#include <bmx/Logging.h>

using namespace std;
using namespace bmx;



static const Rational HEVC_SAMPLE_ASPECT_RATIOS[] =
{
    {0,   0},   // unspecified
    {1,   1},
    {12,  11},
    {10,  11},
    {16,  11},
    {40,  33},
    {24,  11},
    {20,  11},
    {32,  11},
    {80,  33},
    {18,  11},
    {15,  11},
    {64,  33},
    {160, 99},
    {4,   3},
    {3,   2},
    {2,   1},
};



HEVCGetBitBuffer::HEVCGetBitBuffer(const unsigned char *data, uint32_t data_size)
: GetBitBuffer(data, data_size)
{
}

void HEVCGetBitBuffer::GetRBSPBits(uint8_t num_bits, uint64_t *value)
{
    if (num_bits > GetRemBitSize())
        throw false;

    uint8_t check_num_bytes = (uint8_t)(((uint64_t)num_bits + (mBitPos & 7) + 7) >> 3);
    const unsigned char *bytes = &mData[mPos];

    uint8_t prev_bytes = 0;
    if (mPos >= 2 && (mBitPos & 7) == 0)
        prev_bytes = 2;
    else if (mPos >= 1)
        prev_bytes = 1;
    bytes -= prev_bytes;
    check_num_bytes += prev_bytes;

    uint32_t state = 0xffffff;
    uint8_t i;
    for (i = 0; i < check_num_bytes; i++) {
        state = (state << 8) | bytes[i];
        if ((state & 0xffffff) == 0x000003)
            break;
    }

    uint64_t start_bit_pos = mBitPos;
    try
    {
        if (i < check_num_bytes) {
            uint8_t before_bits, after_bits;
            uint64_t before_value, after_value;

            before_bits = ((i - prev_bytes) << 3) - (uint8_t)(mBitPos & 7);
            after_bits = num_bits - before_bits;

            GetBits(before_bits, &before_value);
            SetBitPos(mBitPos + 8);
            GetRBSPBits(after_bits, &after_value);

            *value = (before_value << after_bits) | after_value;
        } else if (!GetBits(num_bits, value)) {
            throw false;
        }
    }
    catch (...)
    {
        SetBitPos(start_bit_pos);
        throw;
    }
}

void HEVCGetBitBuffer::GetF(uint8_t num_bits, uint64_t *value)
{
    GetRBSPBits(num_bits, value);
}

void HEVCGetBitBuffer::GetU(uint8_t num_bits, uint8_t *value)
{
    uint64_t v;
    GetRBSPBits(num_bits, &v);
    *value = (uint8_t)v;
}

void HEVCGetBitBuffer::GetU(uint8_t num_bits, uint64_t *value)
{
    GetRBSPBits(num_bits, value);
}

void HEVCGetBitBuffer::GetUE(uint64_t *value)
{
    int leading_zeros = 0;
    uint64_t bit;
    do {
        GetRBSPBits(1, &bit);
        if (bit == 0)
            leading_zeros++;
    } while (bit == 0 && leading_zeros < 32);

    if (leading_zeros == 0) {
        *value = 0;
    } else {
        uint64_t suffix;
        GetRBSPBits(leading_zeros, &suffix);
        *value = (1ULL << leading_zeros) - 1 + suffix;
    }
}

void HEVCGetBitBuffer::GetSE(int64_t *value)
{
    uint64_t code;
    GetUE(&code);
    if (code & 1)
        *value = (int64_t)((code + 1) >> 1);
    else
        *value = -(int64_t)(code >> 1);
}

bool HEVCGetBitBuffer::MoreRBSPData()
{
    return GetRemBitSize() > 0;
}



HEVCEssenceParser::HEVCEssenceParser()
{
    mProfile = 0;
    mTier = 0;
    mLevel = 0;
    mStoredWidth = 0;
    mStoredHeight = 0;
    mDisplayWidth = 0;
    mDisplayHeight = 0;
    mComponentDepth = 0;
    mChromaFormat = 0;
    mColorPrimaries = 2;
    mTransferCharacteristics = 2;
    mMatrixCoefficients = 2;
    mFrameRate = ZERO_RATIONAL;
    mSampleAspectRatio = ZERO_RATIONAL;
    mFrameType = UNKNOWN_FRAME_TYPE;
    mIsIDRFrame = false;
    mIsCRAFrame = false;
    mOffsetDataReady = false;
    mInFrame = false;
    mFrameSize = 0;
}

HEVCEssenceParser::~HEVCEssenceParser()
{
}

void HEVCEssenceParser::SetVPS(const unsigned char *data, uint32_t size)
{
    ParseVPS(data, size);
}

void HEVCEssenceParser::SetSPS(const unsigned char *data, uint32_t size)
{
    ParseSPS(data, size);
}

void HEVCEssenceParser::SetPPS(const unsigned char *data, uint32_t size)
{
    ParsePPS(data, size);
}

bool HEVCEssenceParser::IsVCLNALType(uint8_t nal_type)
{
    return nal_type <= 31;
}

bool HEVCEssenceParser::IsRandomAccessPoint(uint8_t nal_type)
{
    return nal_type >= HEVC_BLA_W_LP && nal_type <= HEVC_CRA_NUT;
}

bool HEVCEssenceParser::IsIDRNALType(uint8_t nal_type)
{
    return nal_type == HEVC_IDR_W_RADL || nal_type == HEVC_IDR_N_LP;
}

bool HEVCEssenceParser::IsCRANALType(uint8_t nal_type)
{
    return nal_type == HEVC_CRA_NUT;
}

void HEVCEssenceParser::ParseNALUnits(const unsigned char *data, uint32_t size, vector<HEVCNALReference> *nals)
{
    nals->clear();
    uint32_t i = 0;

    while (i < size) {
        // Find start code (0x00 0x00 0x01 or 0x00 0x00 0x00 0x01)
        if (i + 2 < size && data[i] == 0x00 && data[i + 1] == 0x00) {
            uint32_t sc_len = 0;
            if (data[i + 2] == 0x01) {
                sc_len = 3;
            } else if (i + 3 < size && data[i + 2] == 0x00 && data[i + 3] == 0x01) {
                sc_len = 4;
            }

            if (sc_len > 0) {
                uint32_t nal_start = i + sc_len;
                if (nal_start + 1 < size) {
                    // HEVC NAL header: forbidden_zero_bit(1) + nal_unit_type(6) + nuh_layer_id(6) + nuh_temporal_id_plus1(3)
                    uint8_t byte0 = data[nal_start];
                    uint8_t byte1 = data[nal_start + 1];
                    uint8_t nal_type = (byte0 >> 1) & 0x3F;
                    uint8_t nuh_layer_id = ((byte0 & 0x01) << 5) | ((byte1 >> 3) & 0x1F);
                    uint8_t nuh_temporal_id_plus1 = byte1 & 0x07;

                    // Find end of this NAL (next start code or end of data)
                    uint32_t nal_end = nal_start + 2;
                    while (nal_end + 2 < size) {
                        if (data[nal_end] == 0x00 && data[nal_end + 1] == 0x00 &&
                            (data[nal_end + 2] == 0x01 || (nal_end + 3 < size && data[nal_end + 2] == 0x00 && data[nal_end + 3] == 0x01)))
                            break;
                        nal_end++;
                    }
                    if (nal_end + 2 >= size)
                        nal_end = size;

                    HEVCNALReference ref;
                    ref.type = nal_type;
                    ref.nuh_layer_id = nuh_layer_id;
                    ref.nuh_temporal_id_plus1 = nuh_temporal_id_plus1;
                    ref.data = &data[nal_start];
                    ref.size = nal_end - nal_start;
                    nals->push_back(ref);

                    i = nal_end;
                    continue;
                }
            }
        }
        i++;
    }
}

void HEVCEssenceParser::ResetParseFrameSize()
{
    mFrameSize = 0;
    mInFrame = false;
}

uint32_t HEVCEssenceParser::ParseFrameStart(const unsigned char *data, uint32_t data_size)
{
    vector<HEVCNALReference> nals;
    ParseNALUnits(data, data_size, &nals);

    for (size_t i = 0; i < nals.size(); i++) {
        if (IsVCLNALType(nals[i].type) || nals[i].type == HEVC_AUD_NUT) {
            return (uint32_t)(nals[i].data - data);
        }
    }
    return 0;
}

uint32_t HEVCEssenceParser::ParseFrameSize(const unsigned char *data, uint32_t data_size)
{
    vector<HEVCNALReference> nals;
    ParseNALUnits(data, data_size, &nals);

    bool found_vcl = false;
    uint32_t frame_end = data_size;

    for (size_t i = 0; i < nals.size(); i++) {
        if (IsVCLNALType(nals[i].type)) {
            if (found_vcl) {
                // Check if this is the first slice of a new picture (first_slice_segment_in_pic_flag)
                if (nals[i].size > 2) {
                    uint8_t first_byte_after_header = nals[i].data[2];
                    bool first_slice = (first_byte_after_header >> 7) & 1;
                    if (first_slice) {
                        frame_end = (uint32_t)(nals[i].data - data);
                        break;
                    }
                }
            }
            found_vcl = true;
        } else if (found_vcl && (nals[i].type == HEVC_AUD_NUT ||
                                  nals[i].type == HEVC_VPS_NUT ||
                                  nals[i].type == HEVC_SPS_NUT ||
                                  nals[i].type == HEVC_PPS_NUT ||
                                  nals[i].type == HEVC_EOS_NUT ||
                                  nals[i].type == HEVC_EOB_NUT)) {
            frame_end = (uint32_t)(nals[i].data - data);
            break;
        }
    }
    return frame_end;
}

void HEVCEssenceParser::ParseFrameInfo(const unsigned char *data, uint32_t data_size)
{
    mFrameType = UNKNOWN_FRAME_TYPE;
    mIsIDRFrame = false;
    mIsCRAFrame = false;

    vector<HEVCNALReference> nals;
    ParseNALUnits(data, data_size, &nals);

    for (size_t i = 0; i < nals.size(); i++) {
        switch (nals[i].type) {
            case HEVC_VPS_NUT:
                ParseVPS(nals[i].data + 2, nals[i].size - 2);
                break;
            case HEVC_SPS_NUT:
                ParseSPS(nals[i].data + 2, nals[i].size - 2);
                break;
            case HEVC_PPS_NUT:
                ParsePPS(nals[i].data + 2, nals[i].size - 2);
                break;
            default:
                if (IsVCLNALType(nals[i].type)) {
                    if (IsIDRNALType(nals[i].type)) {
                        mFrameType = I_FRAME;
                        mIsIDRFrame = true;
                    } else if (IsCRANALType(nals[i].type)) {
                        mFrameType = I_FRAME;
                        mIsCRAFrame = true;
                    } else if (nals[i].type >= HEVC_BLA_W_LP && nals[i].type <= HEVC_BLA_N_LP) {
                        mFrameType = I_FRAME;
                    } else {
                        // Determine P or B from slice type in slice header
                        ParseSliceHeader(nals[i].data + 2, nals[i].size - 2, nals[i].type);
                    }
                    return;
                }
                break;
        }
    }
}

void HEVCEssenceParser::ParseSliceHeader(const unsigned char *data, uint32_t size, uint8_t nal_type)
{
    if (size < 1)
        return;

    try {
        HEVCGetBitBuffer reader(data, size);

        uint64_t first_slice_segment_in_pic_flag;
        reader.GetF(1, &first_slice_segment_in_pic_flag);

        if (IsRandomAccessPoint(nal_type)) {
            uint64_t no_output_of_prior_pics_flag;
            reader.GetF(1, &no_output_of_prior_pics_flag);
        }

        uint64_t pps_id;
        reader.GetUE(&pps_id);

        if (mPPSMap.find((uint8_t)pps_id) == mPPSMap.end())
            return;

        const PPS &pps = mPPSMap[(uint8_t)pps_id];

        if (!first_slice_segment_in_pic_flag && pps.dependent_slice_segments_enabled_flag) {
            uint64_t dependent_slice_segment_flag;
            reader.GetF(1, &dependent_slice_segment_flag);
        }

        // Skip slice_segment_address bits if not first slice
        // For now, only parse first slice of picture for slice_type

        uint64_t slice_type = 2; // default I
        if (pps.num_extra_slice_header_bits > 0) {
            uint64_t dummy;
            reader.GetF(pps.num_extra_slice_header_bits, &dummy);
        }

        reader.GetUE(&slice_type);

        switch (slice_type) {
            case 0: mFrameType = B_FRAME; break;
            case 1: mFrameType = P_FRAME; break;
            case 2: mFrameType = I_FRAME; break;
            default: mFrameType = UNKNOWN_FRAME_TYPE; break;
        }
    } catch (...) {
        // Bitstream parsing error — leave frame type as unknown
    }
}

void HEVCEssenceParser::ParseProfileTierLevel(HEVCGetBitBuffer &reader, ProfileTierLevel *ptl, uint8_t max_sub_layers_minus1)
{
    uint64_t u64;

    reader.GetU(2, &ptl->general_profile_space);
    reader.GetU(1, &ptl->general_tier_flag);
    reader.GetU(5, &ptl->general_profile_idc);

    // general_profile_compatibility_flag[32]
    for (int i = 0; i < 32; i++) {
        reader.GetF(1, &u64);
    }

    // general_progressive_source_flag .. general_reserved_zero_43bits
    reader.GetF(48, &u64);

    reader.GetU(8, &ptl->general_level_idc);

    // sub_layer_profile_present_flag and sub_layer_level_present_flag
    for (uint8_t i = 0; i < max_sub_layers_minus1; i++) {
        reader.GetF(1, &u64); // sub_layer_profile_present_flag
        reader.GetF(1, &u64); // sub_layer_level_present_flag
    }

    if (max_sub_layers_minus1 > 0) {
        for (uint8_t i = max_sub_layers_minus1; i < 8; i++) {
            reader.GetF(2, &u64); // reserved_zero_2bits
        }
    }

    // Skip sub-layer profile/tier/level data (simplified — we only use general)
}

void HEVCEssenceParser::ParseVPS(const unsigned char *data, uint32_t size)
{
    try {
        HEVCGetBitBuffer reader(data, size);
        VPS vps;
        memset(&vps, 0, sizeof(vps));

        uint64_t u64;
        reader.GetU(4, &vps.vps_id);
        reader.GetF(6, &u64); // vps_reserved_three_2bits + vps_max_layers_minus1(partial)
        vps.vps_max_layers_minus1 = (uint8_t)((u64 >> 2) & 0x3F);
        reader.GetU(3, &vps.vps_max_sub_layers_minus1);
        reader.GetU(1, &vps.vps_temporal_id_nesting_flag);
        reader.GetF(16, &u64); // vps_reserved_0xffff_16bits

        ParseProfileTierLevel(reader, &vps.ptl, vps.vps_max_sub_layers_minus1);

        mVPSMap[vps.vps_id] = vps;
    } catch (...) {
    }
}

void HEVCEssenceParser::ParseSPS(const unsigned char *data, uint32_t size)
{
    try {
        HEVCGetBitBuffer reader(data, size);
        SPS sps;
        memset(&sps, 0, sizeof(sps));

        uint64_t u64;
        reader.GetU(4, &sps.vps_id);
        reader.GetU(3, &sps.max_sub_layers_minus1);
        reader.GetU(1, &sps.temporal_id_nesting_flag);

        ParseProfileTierLevel(reader, &sps.ptl, sps.max_sub_layers_minus1);

        reader.GetUE(&u64); sps.sps_id = (uint8_t)u64;
        reader.GetUE(&u64); sps.chroma_format_idc = (uint8_t)u64;

        if (sps.chroma_format_idc == 3) {
            reader.GetU(1, &sps.separate_colour_plane_flag);
        }

        reader.GetUE(&u64); sps.pic_width_in_luma_samples = (uint32_t)u64;
        reader.GetUE(&u64); sps.pic_height_in_luma_samples = (uint32_t)u64;

        uint8_t conformance_window_flag;
        reader.GetU(1, &conformance_window_flag);
        sps.conformance_window_flag = conformance_window_flag;

        if (conformance_window_flag) {
            reader.GetUE(&u64); sps.conf_win_left_offset = (uint32_t)u64;
            reader.GetUE(&u64); sps.conf_win_right_offset = (uint32_t)u64;
            reader.GetUE(&u64); sps.conf_win_top_offset = (uint32_t)u64;
            reader.GetUE(&u64); sps.conf_win_bottom_offset = (uint32_t)u64;
        }

        reader.GetUE(&u64); sps.bit_depth_luma_minus8 = (uint8_t)u64;
        reader.GetUE(&u64); sps.bit_depth_chroma_minus8 = (uint8_t)u64;
        reader.GetUE(&sps.log2_max_pic_order_cnt_lsb_minus4);

        // Skip sub_layer_ordering_info
        uint8_t sps_sub_layer_ordering_info_present_flag;
        reader.GetU(1, &sps_sub_layer_ordering_info_present_flag);
        for (uint8_t i = (sps_sub_layer_ordering_info_present_flag ? 0 : sps.max_sub_layers_minus1);
             i <= sps.max_sub_layers_minus1; i++) {
            reader.GetUE(&u64); // max_dec_pic_buffering_minus1
            reader.GetUE(&u64); // max_num_reorder_pics
            reader.GetUE(&u64); // max_latency_increase_plus1
        }

        reader.GetUE(&u64); // log2_min_luma_coding_block_size_minus3
        reader.GetUE(&u64); // log2_diff_max_min_luma_coding_block_size
        reader.GetUE(&u64); // log2_min_luma_transform_block_size_minus2
        reader.GetUE(&u64); // log2_diff_max_min_luma_transform_block_size
        reader.GetUE(&u64); // max_transform_hierarchy_depth_inter
        reader.GetUE(&u64); // max_transform_hierarchy_depth_intra

        uint8_t scaling_list_enabled_flag;
        reader.GetU(1, &scaling_list_enabled_flag);
        if (scaling_list_enabled_flag) {
            uint8_t sps_scaling_list_data_present_flag;
            reader.GetU(1, &sps_scaling_list_data_present_flag);
            // Skip scaling list data if present (complex nested loops)
        }

        reader.GetF(1, &u64); // amp_enabled_flag
        reader.GetF(1, &u64); // sample_adaptive_offset_enabled_flag

        uint8_t pcm_enabled_flag;
        reader.GetU(1, &pcm_enabled_flag);
        if (pcm_enabled_flag) {
            reader.GetF(4, &u64); // pcm_sample_bit_depth_luma_minus1
            reader.GetF(4, &u64); // pcm_sample_bit_depth_chroma_minus1
            reader.GetUE(&u64);   // log2_min_pcm_luma_coding_block_size_minus3
            reader.GetUE(&u64);   // log2_diff_max_min_pcm_luma_coding_block_size
            reader.GetF(1, &u64); // pcm_loop_filter_disabled_flag
        }

        uint64_t num_short_term_ref_pic_sets;
        reader.GetUE(&num_short_term_ref_pic_sets);
        // Skip short-term ref pic sets (complex — for now we stop SPS parsing here)

        // Populate extracted values
        mProfile = sps.ptl.general_profile_idc;
        mTier = sps.ptl.general_tier_flag;
        mLevel = sps.ptl.general_level_idc;

        mStoredWidth = sps.pic_width_in_luma_samples;
        mStoredHeight = sps.pic_height_in_luma_samples;

        uint32_t sub_width_c = (sps.chroma_format_idc == 1 || sps.chroma_format_idc == 2) ? 2 : 1;
        uint32_t sub_height_c = (sps.chroma_format_idc == 1) ? 2 : 1;

        if (sps.conformance_window_flag) {
            mDisplayWidth = sps.pic_width_in_luma_samples -
                            (sps.conf_win_left_offset + sps.conf_win_right_offset) * sub_width_c;
            mDisplayHeight = sps.pic_height_in_luma_samples -
                             (sps.conf_win_top_offset + sps.conf_win_bottom_offset) * sub_height_c;
        } else {
            mDisplayWidth = mStoredWidth;
            mDisplayHeight = mStoredHeight;
        }

        mComponentDepth = 8 + sps.bit_depth_luma_minus8;
        mChromaFormat = sps.chroma_format_idc;

        mSPSMap[sps.sps_id] = sps;

    } catch (...) {
    }
}

void HEVCEssenceParser::ParsePPS(const unsigned char *data, uint32_t size)
{
    try {
        HEVCGetBitBuffer reader(data, size);
        PPS pps;
        memset(&pps, 0, sizeof(pps));

        uint64_t u64;
        reader.GetUE(&u64); pps.pps_id = (uint8_t)u64;
        reader.GetUE(&u64); pps.sps_id = (uint8_t)u64;
        reader.GetU(1, &pps.dependent_slice_segments_enabled_flag);
        reader.GetU(1, &pps.output_flag_present_flag);
        reader.GetU(3, &pps.num_extra_slice_header_bits);

        mPPSMap[pps.pps_id] = pps;

    } catch (...) {
    }
}

EssenceType HEVCEssenceParser::GetEssenceType() const
{
    if (mSPSMap.empty())
        return HEVC_MAIN_10;

    const SPS &sps = mSPSMap.begin()->second;
    uint8_t profile = sps.ptl.general_profile_idc;
    uint8_t depth = 8 + sps.bit_depth_luma_minus8;
    uint8_t chroma = sps.chroma_format_idc;

    switch (profile) {
        case 1: // Main
            return HEVC_MAIN;
        case 2: // Main 10
            if (chroma == 2) return (depth >= 12) ? HEVC_MAIN_422_12 : HEVC_MAIN_422_10;
            if (chroma == 3) return (depth >= 12) ? HEVC_MAIN_444_12 : HEVC_MAIN_444_10;
            return (depth >= 12) ? HEVC_MAIN_12 : HEVC_MAIN_10;
        case 3: // Main Still Picture / Intra
            return HEVC_MAIN_INTRA;
        case 4: // Format range extensions
            if (chroma == 2) return (depth >= 12) ? HEVC_MAIN_422_12_INTRA : HEVC_MAIN_422_10_INTRA;
            if (chroma == 3) return (depth >= 12) ? HEVC_MAIN_444_12_INTRA : HEVC_MAIN_444_10_INTRA;
            return (depth >= 12) ? HEVC_MAIN_12_INTRA : HEVC_MAIN_10_INTRA;
        default:
            return HEVC_MAIN_10;
    }
}
