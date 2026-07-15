/*
 * Copyright (C) 2010, British Broadcasting Corporation
 * All Rights Reserved.
 *
 * Author: Philip de Nier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the British Broadcasting Corporation nor the names
 *       of its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <bmx/essence_parser/VC3EssenceParser.h>
#include "EssenceParserUtils.h"
#include <bmx/Utils.h>
#include <bmx/BMXException.h>
#include <bmx/Logging.h>

using namespace std;
using namespace bmx;


#define HEADER_PREFIX_HVN0  0x000000028000LL
#define HEADER_PREFIX_S     0x000002800000LL


typedef struct
{
    uint32_t compression_id;
    bool is_progressive;
    uint16_t frame_width;
    uint16_t frame_height;
    uint8_t bit_depth;
    uint32_t frame_size;
} CompressionParameters;

static const CompressionParameters COMPRESSION_PARAMETERS[] =
{
    {1235,  true,   1920,   1080,   10,  917504},
    {1237,  true,   1920,   1080,   8,   606208},
    {1238,  true,   1920,   1080,   8,   917504},
    {1241,  false,  1920,   1080,   10,  917504},
    {1242,  false,  1920,   1080,   8,   606208},
    {1243,  false,  1920,   1080,   8,   917504},
    {1244,  false,  1920,   1080,   8,   606208},
    {1250,  true,   1280,   720,    10,  458752},
    {1251,  true,   1280,   720,    8,   458752},
    {1252,  true,   1280,   720,    8,   303104},
    {1253,  true,   1920,   1080,   8,   188416},
    {1258,  true,   1280,   720,    8,   212992},
    {1259,  true,   1920,   1080,   8,   417792},
    {1260,  false,  1920,   1080,   8,   417792},
};


// GKX (GKX-122): VC-3 / DNxHR resolution-independent compression IDs 1270-1274.
// DNxHR is resolution-INDEPENDENT, so unlike the fixed-raster DNxHD table above the
// raster and constant frame size are NOT a function of the compression id alone --
// they depend on the actual raster (read from the frame header) with the frame size
// keyed by (profile, width). Ported from analyzer_vc3::set_info_dnxhr.
static bool is_dnxhr_id(uint32_t compression_id)
{
    return compression_id >= 1270 && compression_id <= 1274;
}

static uint32_t dnxhr_nominal_bit_depth(uint32_t compression_id)
{
    // 444 (1270) and HQX (1271) are 10-bit; HQ/SQ/LB (1272/1273/1274) are 8-bit.
    return (compression_id == 1270 || compression_id == 1271) ? 10 : 8;
}

// (profile, width) -> constant frame size, verbatim from analyzer_vc3::set_info_dnxhr.
// Returns 0 for a width outside the supported set (1920/2048/3840/4096).
static uint32_t dnxhr_frame_size(uint32_t compression_id, uint16_t width)
{
    switch (compression_id) {
        case 1270:  // 444
            if (width == 1920) return 0x1c0000;
            if (width == 2048) return 1941504;
            if (width == 3840) return 7286784;
            if (width == 4096) return 7770112;
            break;
        case 1271:  // HQX (10-bit)
        case 1272:  // HQ  (8-bit)
            if (width == 1920) return 0xe0000;
            if (width == 2048) return 970752;
            if (width == 3840) return 3641344;
            if (width == 4096) return 3887104;
            break;
        case 1273:  // SQ
            if (width == 1920) return 0x94000;
            if (width == 2048) return 643072;
            if (width == 3840) return 2408448;
            if (width == 4096) return 2568192;
            break;
        case 1274:  // LB
            if (width == 1920) return 0x2E000;
            if (width == 2048) return 200704;
            if (width == 3840) return 749568;
            if (width == 4096) return 798720;
            break;
        default:
            break;
    }
    return 0;
}



static uint64_t get_uint64(const unsigned char *data)
{
    return (((uint64_t)data[0]) << 56) |
           (((uint64_t)data[1]) << 48) |
           (((uint64_t)data[2]) << 40) |
           (((uint64_t)data[3]) << 32) |
           (((uint64_t)data[4]) << 24) |
           (((uint64_t)data[5]) << 16) |
           (((uint64_t)data[6]) << 8) |
             (uint64_t)data[7];
}

static uint32_t get_uint32(const unsigned char *data)
{
    return (((uint32_t)data[0]) << 24) |
           (((uint32_t)data[1]) << 16) |
           (((uint32_t)data[2]) << 8) |
             (uint32_t)data[3];
}

static uint16_t get_uint16(const unsigned char *data)
{
    return (((uint16_t)data[0]) << 8) |
             (uint16_t)data[1];
}



VC3EssenceParser::VC3EssenceParser()
{
    mCompressionId = 0;
    mIsProgressive = false;
    mFrameWidth = 0;
    mFrameHeight = 0;
    mBitDepth = 0;
    mFrameSize = 0;
}

VC3EssenceParser::~VC3EssenceParser()
{
}

uint32_t VC3EssenceParser::ParseFrameStart(const unsigned char *data, uint32_t data_size)
{
    BMX_CHECK(data_size != ESSENCE_PARSER_NULL_OFFSET);

    uint64_t state = 0;
    uint32_t i;
    for (i = 0; i < data_size; i++) {
        state = (state << 8) | data[i];
        if ((state & 0xffffffff0000LL) == HEADER_PREFIX_S &&
            (state & 0x000000000003LL) < 3)    // coding unit is progressive frame or field 1
        {
            return i - 5;
        }
    }

    return ESSENCE_PARSER_NULL_OFFSET;
}

void VC3EssenceParser::ResetParseFrameSize()
{
}

uint32_t VC3EssenceParser::ParseFrameSize(const unsigned char *data, uint32_t data_size)
{
    BMX_CHECK(data_size != ESSENCE_PARSER_NULL_OFFSET);

    if (data_size < VC3_PARSER_MIN_DATA_SIZE)
        return ESSENCE_PARSER_NULL_OFFSET;

    // check header prefix
    uint64_t prefix = get_uint64(data) >> 24;
    BMX_CHECK( (prefix & 0xffffffff00LL) == HEADER_PREFIX_HVN0 &&
              ((prefix & 0x00000000ffLL) == 1 || (prefix & 0x00000000ffLL) == 2));

    uint32_t compression_id = get_uint32(data + 40);

    size_t i;
    for (i = 0; i < BMX_ARRAY_SIZE(COMPRESSION_PARAMETERS); i++)
    {
        if (compression_id == COMPRESSION_PARAMETERS[i].compression_id)
            return COMPRESSION_PARAMETERS[i].frame_size;
    }

    // GKX (GKX-122): resolution-independent DNxHR -- frame size is keyed by
    // (profile, width); width is read from the frame header (samples per line).
    if (is_dnxhr_id(compression_id)) {
        uint16_t width = get_uint16(data + 26);
        uint32_t frame_size = dnxhr_frame_size(compression_id, width);
        if (frame_size != 0)
            return frame_size;
        // Unsupported raster width -- surface as "unknown" rather than a bogus size.
        return ESSENCE_PARSER_NULL_FRAME_SIZE;
    }

    return ESSENCE_PARSER_NULL_FRAME_SIZE;
}

void VC3EssenceParser::ParseFrameInfo(const unsigned char *data, uint32_t data_size)
{
    BMX_CHECK(data_size != ESSENCE_PARSER_NULL_OFFSET);
    BMX_CHECK(data_size >= VC3_PARSER_MIN_DATA_SIZE);

    // check header prefix
    uint64_t prefix = get_uint64(data) >> 24;
    BMX_CHECK( (prefix & 0xffffffff00LL) == HEADER_PREFIX_HVN0 &&
              ((prefix & 0x00000000ffLL) == 1 || (prefix & 0x00000000ffLL) == 2));

    // compression id
    mCompressionId = get_uint32(data + 40);

    // GKX (GKX-122): resolution-independent DNxHR (1270-1274). The raster is NOT a
    // function of the compression id, so read the geometry/depth from the frame
    // header instead of a fixed table. SBD (sub-sampling bit depth) at bit offset
    // 33*8 gives 10-bit (2) or 8-bit (1); SPL (samples per line) at +26 gives width;
    // ALPF (active lines per frame) at +24 gives height. The constant frame size is
    // keyed by (profile, width). SST (bit 0 of byte 5, checked in ParseFrameStart)
    // distinguishes progressive (field 1 == 0) from interlaced.
    if (is_dnxhr_id(mCompressionId)) {
        uint8_t sst = (uint8_t)(data[5] & 0x03);
        mIsProgressive = (sst == 0);
        mFrameWidth = get_uint16(data + 26);
        mFrameHeight = get_uint16(data + 24);
        BMX_CHECK_M(mFrameHeight != 0,
                    ("DNxHR ALPF (active-lines-per-frame / height) is zero in the frame header"));
        uint32_t sbd_bits = get_bits(data, data_size, 33 * 8, 3);
        // Honour the header's actual bit depth; fall back to the profile-nominal depth.
        if (sbd_bits == 2)
            mBitDepth = 10;
        else if (sbd_bits == 1)
            mBitDepth = 8;
        else
            mBitDepth = (uint8_t)dnxhr_nominal_bit_depth(mCompressionId);
        mFrameSize = dnxhr_frame_size(mCompressionId, mFrameWidth);
        BMX_CHECK_M(mFrameSize != 0,
                    ("Unsupported DNxHR raster width %u (compression id %u); supported widths are "
                     "1920, 2048, 3840, 4096", mFrameWidth, mCompressionId));
        return;
    }

    size_t param_index;
    for (param_index = 0; param_index < BMX_ARRAY_SIZE(COMPRESSION_PARAMETERS); param_index++)
    {
        if (mCompressionId == COMPRESSION_PARAMETERS[param_index].compression_id)
            break;
    }
    BMX_CHECK(param_index < BMX_ARRAY_SIZE(COMPRESSION_PARAMETERS));

    // Note: found that an Avid MC v3.0 file containing 1252 720p50 had FFC=01h and SST=1;
    //       SST should be 0 for progressive scan. DNxHD_Compliance_Issue_To_Licensees-1.doc
    //       states that some Avid bitstreams may have SST incorrectly set to 1080i
    //       Ignore the bitstream information and use the scan type associated with the compression id
    mIsProgressive = COMPRESSION_PARAMETERS[param_index].is_progressive;

    // image geometry
    // Note: DNxHD_Compliance_Issue_To_Licensees-1.doc states that some Avid bitstreams,
    //       e.g. produced by Avid Media Composer 3.0, may have ALPF incorrectly set to 1080 for
    //       1080i sources. Ignore the bitstream information and use the frame height associated
    //       with the compression id
    mFrameHeight = COMPRESSION_PARAMETERS[param_index].frame_height;
    mFrameWidth = get_uint16(data + 26);
    BMX_CHECK(mFrameWidth == COMPRESSION_PARAMETERS[param_index].frame_width);
    uint32_t sbd_bits = get_bits(data, data_size, 33 * 8, 3);
    BMX_CHECK(sbd_bits == 2 || sbd_bits == 1);
    mBitDepth = (sbd_bits == 2 ? 10 : 8);
    BMX_CHECK(mBitDepth == COMPRESSION_PARAMETERS[param_index].bit_depth);

    mFrameSize = COMPRESSION_PARAMETERS[param_index].frame_size;
}

