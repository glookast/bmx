/*
 * Copyright (C) 2026, Glookast LLC
 * All Rights Reserved.
 *
 * HEVC/H.265 Essence Parser for MXF OP1a writing.
 * Parses HEVC NAL units to extract VPS/SPS/PPS, detect frame boundaries
 * and frame types, and calculate picture order count for index tables.
 */

#ifndef BMX_HEVC_ESSENCE_PARSER_H_
#define BMX_HEVC_ESSENCE_PARSER_H_

#include <map>
#include <vector>

#include <bmx/essence_parser/EssenceParser.h>
#include <bmx/BitBuffer.h>
#include <bmx/EssenceType.h>


namespace bmx
{


typedef enum
{
    HEVC_TRAIL_N        = 0,
    HEVC_TRAIL_R        = 1,
    HEVC_TSA_N          = 2,
    HEVC_TSA_R          = 3,
    HEVC_STSA_N         = 4,
    HEVC_STSA_R         = 5,
    HEVC_RADL_N         = 6,
    HEVC_RADL_R         = 7,
    HEVC_RASL_N         = 8,
    HEVC_RASL_R         = 9,
    HEVC_RSV_VCL_N10    = 10,
    HEVC_RSV_VCL_R11    = 11,
    HEVC_RSV_VCL_N12    = 12,
    HEVC_RSV_VCL_R13    = 13,
    HEVC_RSV_VCL_N14    = 14,
    HEVC_RSV_VCL_R15    = 15,
    HEVC_BLA_W_LP       = 16,
    HEVC_BLA_W_RADL     = 17,
    HEVC_BLA_N_LP       = 18,
    HEVC_IDR_W_RADL     = 19,
    HEVC_IDR_N_LP       = 20,
    HEVC_CRA_NUT        = 21,
    HEVC_VPS_NUT        = 32,
    HEVC_SPS_NUT        = 33,
    HEVC_PPS_NUT        = 34,
    HEVC_AUD_NUT        = 35,
    HEVC_EOS_NUT        = 36,
    HEVC_EOB_NUT        = 37,
    HEVC_FD_NUT         = 38,
    HEVC_PREFIX_SEI_NUT = 39,
    HEVC_SUFFIX_SEI_NUT = 40,
} HEVCNALUnitType;

typedef struct
{
    uint8_t type;
    uint8_t nuh_layer_id;
    uint8_t nuh_temporal_id_plus1;
    const unsigned char *data;
    uint32_t size;
} HEVCNALReference;


class HEVCGetBitBuffer : public GetBitBuffer
{
public:
    HEVCGetBitBuffer(const unsigned char *data, uint32_t data_size);

    void GetF(uint8_t num_bits, uint64_t *value);
    void GetU(uint8_t num_bits, uint8_t *value);
    void GetU(uint8_t num_bits, uint64_t *value);
    void GetUE(uint64_t *value);
    void GetSE(int64_t *value);
    bool MoreRBSPData();

private:
    void GetRBSPBits(uint8_t num_bits, uint64_t *value);
};


class HEVCEssenceParser : public EssenceParser
{
public:
    HEVCEssenceParser();
    virtual ~HEVCEssenceParser();

    void SetVPS(const unsigned char *data, uint32_t size);
    void SetSPS(const unsigned char *data, uint32_t size);
    void SetPPS(const unsigned char *data, uint32_t size);

    virtual uint32_t ParseFrameStart(const unsigned char *data, uint32_t data_size);
    virtual void ResetParseFrameSize();
    virtual uint32_t ParseFrameSize(const unsigned char *data, uint32_t data_size);

    void ParseFrameInfo(const unsigned char *data, uint32_t data_size);

    void ParseNALUnits(const unsigned char *data, uint32_t size, std::vector<HEVCNALReference> *nals);

public:
    uint8_t GetProfile() const               { return mProfile; }
    uint8_t GetTier() const                  { return mTier; }
    uint8_t GetLevel() const                 { return mLevel; }
    uint32_t GetStoredWidth() const          { return mStoredWidth; }
    uint32_t GetStoredHeight() const         { return mStoredHeight; }
    uint32_t GetDisplayWidth() const         { return mDisplayWidth; }
    uint32_t GetDisplayHeight() const        { return mDisplayHeight; }
    uint32_t GetComponentDepth() const       { return mComponentDepth; }
    uint8_t GetChromaFormat() const          { return mChromaFormat; }
    uint8_t GetColorPrimaries() const        { return mColorPrimaries; }
    uint8_t GetTransferCharacteristics() const { return mTransferCharacteristics; }
    uint8_t GetMatrixCoefficients() const    { return mMatrixCoefficients; }
    MPEGFrameType GetFrameType() const       { return mFrameType; }
    bool IsIDRFrame() const                  { return mIsIDRFrame; }
    bool IsCRAFrame() const                  { return mIsCRAFrame; }
    bool HaveFrameRate() const               { return mFrameRate.numerator > 0; }
    Rational GetFrameRate() const            { return mFrameRate; }
    Rational GetSampleAspectRatio() const    { return mSampleAspectRatio; }

    EssenceType GetEssenceType() const;

    bool HaveSequenceParameterSet() const    { return !mSPSMap.empty(); }
    bool HaveVideoParameterSet() const       { return !mVPSMap.empty(); }

private:
    typedef struct
    {
        uint8_t general_profile_space;
        uint8_t general_tier_flag;
        uint8_t general_profile_idc;
        uint8_t general_level_idc;
    } ProfileTierLevel;

    typedef struct
    {
        uint8_t vps_id;
        uint8_t vps_max_layers_minus1;
        uint8_t vps_max_sub_layers_minus1;
        uint8_t vps_temporal_id_nesting_flag;
        ProfileTierLevel ptl;
    } VPS;

    typedef struct
    {
        uint8_t sps_id;
        uint8_t vps_id;
        uint8_t max_sub_layers_minus1;
        uint8_t temporal_id_nesting_flag;
        ProfileTierLevel ptl;
        uint8_t chroma_format_idc;
        uint8_t separate_colour_plane_flag;
        uint32_t pic_width_in_luma_samples;
        uint32_t pic_height_in_luma_samples;
        uint8_t conformance_window_flag;
        uint32_t conf_win_left_offset;
        uint32_t conf_win_right_offset;
        uint32_t conf_win_top_offset;
        uint32_t conf_win_bottom_offset;
        uint8_t bit_depth_luma_minus8;
        uint8_t bit_depth_chroma_minus8;
        uint64_t log2_max_pic_order_cnt_lsb_minus4;
        uint8_t vui_parameters_present_flag;
        uint8_t aspect_ratio_info_present_flag;
        uint8_t aspect_ratio_idc;
        uint16_t sar_width;
        uint16_t sar_height;
        uint8_t video_signal_type_present_flag;
        uint8_t video_format;
        uint8_t video_full_range_flag;
        uint8_t colour_description_present_flag;
        uint8_t colour_primaries;
        uint8_t transfer_characteristics;
        uint8_t matrix_coefficients;
        uint8_t timing_info_present_flag;
        uint32_t num_units_in_tick;
        uint32_t time_scale;
    } SPS;

    typedef struct
    {
        uint8_t pps_id;
        uint8_t sps_id;
        uint8_t dependent_slice_segments_enabled_flag;
        uint8_t output_flag_present_flag;
        uint8_t num_extra_slice_header_bits;
    } PPS;

    void ParseVPS(const unsigned char *data, uint32_t size);
    void ParseSPS(const unsigned char *data, uint32_t size);
    void ParsePPS(const unsigned char *data, uint32_t size);
    void ParseProfileTierLevel(HEVCGetBitBuffer &reader, ProfileTierLevel *ptl, uint8_t max_sub_layers_minus1);
    void ParseSliceHeader(const unsigned char *data, uint32_t size, uint8_t nal_type);

    static bool IsVCLNALType(uint8_t nal_type);
    static bool IsRandomAccessPoint(uint8_t nal_type);
    static bool IsIDRNALType(uint8_t nal_type);
    static bool IsCRANALType(uint8_t nal_type);

private:
    std::map<uint8_t, VPS> mVPSMap;
    std::map<uint8_t, SPS> mSPSMap;
    std::map<uint8_t, PPS> mPPSMap;

    uint8_t mProfile;
    uint8_t mTier;
    uint8_t mLevel;
    uint32_t mStoredWidth;
    uint32_t mStoredHeight;
    uint32_t mDisplayWidth;
    uint32_t mDisplayHeight;
    uint32_t mComponentDepth;
    uint8_t mChromaFormat;
    uint8_t mColorPrimaries;
    uint8_t mTransferCharacteristics;
    uint8_t mMatrixCoefficients;
    Rational mFrameRate;
    Rational mSampleAspectRatio;

    MPEGFrameType mFrameType;
    bool mIsIDRFrame;
    bool mIsCRAFrame;
    bool mOffsetDataReady;
    bool mInFrame;
    uint32_t mFrameSize;
};


};


#endif
