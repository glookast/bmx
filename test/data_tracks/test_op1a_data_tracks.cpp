/*
 * Copyright (C) 2026, Glookast LLC
 * All Rights Reserved.
 *
 * Unit tests for ST 436 data tracks in MXF OP1a (MPS-1297)
 *
 * An OP1a may carry one ST 436 VBI track and one ST 436 ANC track. Each test writes a clip with an
 * uncompressed picture track and its data tracks, then walks the file's KLVs and checks that:
 * - every body element's track number is one a file package track states;
 * - each data track has one element per content package, carrying its own payload, ahead of the picture;
 * - the VBI and ANC data descriptors are linked to the tracks carrying VBI and ANC elements;
 * - the essence containers of both data tracks are listed;
 * - the index table locates every element of every content package;
 * - bmx's MXFFileReader reads each data track back, every frame with its own payload;
 * - a single data track keeps the key and track number the unpatched writer gave it
 *   (ANC 0x17010201, VBI 0x17010101);
 * - two data tracks follow SMPTE ST 379-1: element count 2, element numbers 1 and 2 by the order added.
 * RDD 9 allows a single data track: a second one is refused by name when it is added.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <bmx/BMXException.h>
#include <bmx/EssenceType.h>
#include <bmx/mxf_op1a/OP1AFile.h>
#include <bmx/mxf_op1a/OP1ADataTrack.h>
#include <bmx/mxf_reader/MXFFileReader.h>
#include <bmx/rdd9_mxf/RDD9File.h>

#include <libMXF++/MXF.h>

#include <mxf/mxf_labels_and_keys.h>
#include <mxf/mxf_essence_container.h>
#include <mxf/mxf_index_table.h>

using namespace std;
using namespace bmx;
using namespace mxfpp;


static int test_count = 0;
static int pass_count = 0;

#define TEST(name) \
    do { test_count++; printf("  TEST: %s ... ", name); fflush(stdout); } while(0)
#define PASS() \
    do { pass_count++; printf("PASS\n"); } while(0)
#define CHECK(cond, ...) \
    do { if (!(cond)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); return false; } } while(0)


static const uint32_t FRAMES = 5;
static const mxfRational FRAME_RATE = {25, 1};

static const uint8_t GC_DATA_ITEM_TYPE   = 0x17;
static const uint8_t ST436_VBI_ELEMENT   = 0x01;
static const uint8_t ST436_ANC_ELEMENT   = 0x02;


static void push16(vector<uint8_t> *d, uint16_t v)
{
    d->push_back((uint8_t)(v >> 8));
    d->push_back((uint8_t)v);
}

static void push32(vector<uint8_t> *d, uint32_t v)
{
    push16(d, (uint16_t)(v >> 16));
    push16(d, (uint16_t)v);
}

static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t be32(const uint8_t *p) { return ((uint32_t)be16(p) << 16) | be16(p + 2); }
static uint64_t be64(const uint8_t *p) { return ((uint64_t)be32(p) << 32) | be32(p + 4); }

// An ST 436 element of one line whose payload marks the track kind and the frame.
// VBI and ANC payloads differ in size so that a swapped element cannot match.
static vector<uint8_t> st436_payload(EssenceType type, uint32_t frame)
{
    bool vbi = (type == VBI_DATA);
    uint32_t payload_size = vbi ? 8 : 16;

    vector<uint8_t> d;
    push16(&d, 1);                  // number of lines
    push16(&d, vbi ? 21 : 9);       // line number
    d.push_back(1);                 // wrapping type: frame
    d.push_back(4);                 // payload sample coding: 8-bit luma samples
    push16(&d, (uint16_t)payload_size);
    push32(&d, payload_size);       // array element count
    push32(&d, 1);                  // array element size
    d.push_back(vbi ? 'V' : 'A');
    d.push_back(vbi ? ST436_VBI_ELEMENT : ST436_ANC_ELEMENT);
    push32(&d, frame);
    for (uint32_t i = 6; i < payload_size; i++)
        d.push_back((uint8_t)(0xa0 + i));
    return d;
}

static uint8_t element_type_of(EssenceType type)
{
    return type == VBI_DATA ? ST436_VBI_ELEMENT : ST436_ANC_ELEMENT;
}

static bool write_clip(const string &filename, const vector<EssenceType> &data_types, bool constant_size)
{
    try {
        OP1AFile clip(OP1A_DEFAULT_FLAVOUR, File::openNew(filename), FRAME_RATE);

        OP1ATrack *picture = clip.CreateTrack(UNC_SD);
        vector<OP1ATrack*> data_tracks;
        for (size_t i = 0; i < data_types.size(); i++) {
            OP1ATrack *track = clip.CreateTrack(data_types[i]);
            if (constant_size) {
                OP1ADataTrack *data_track = dynamic_cast<OP1ADataTrack*>(track);
                CHECK(data_track, "%s track is not a data track", essence_type_to_string(data_types[i]));
                data_track->SetConstantDataSize((uint32_t)st436_payload(data_types[i], 0).size());
            }
            data_tracks.push_back(track);
        }

        clip.PrepareWrite();

        vector<uint8_t> picture_frame(picture->GetSampleSize(), 0x80);
        for (uint32_t frame = 0; frame < FRAMES; frame++) {
            picture_frame[0] = 'P';
            picture_frame[1] = (uint8_t)frame;
            picture->WriteSamples(&picture_frame[0], (uint32_t)picture_frame.size(), 1);
            for (size_t i = 0; i < data_tracks.size(); i++) {
                vector<uint8_t> payload = st436_payload(data_types[i], frame);
                data_tracks[i]->WriteSamples(&payload[0], (uint32_t)payload.size(), 1);
            }
        }

        clip.CompleteWrite();
    } catch (const BMXException &ex) {
        CHECK(false, "writing %s: %s", filename.c_str(), ex.what());
    } catch (...) {
        CHECK(false, "writing %s: unexpected exception", filename.c_str());
    }

    return true;
}


struct HeaderInfo
{
    map<uint32_t, uint32_t> file_track_id_by_number;   // file package essence tracks
    uint32_t material_data_tracks;
    vector<uint32_t> vbi_linked_track_ids;
    vector<uint32_t> anc_linked_track_ids;
    bool have_vbi_ec;
    bool have_anc_ec;
};

static bool read_header(const string &filename, HeaderInfo *info)
{
    info->material_data_tracks = 0;
    info->have_vbi_ec = false;
    info->have_anc_ec = false;

    File *file = 0;
    try {
        DataModel data_model;
        file = File::openRead(filename);
        CHECK(file->readHeaderPartition(), "%s has no header partition", filename.c_str());

        mxfKey key;
        uint8_t llen;
        uint64_t len;
        file->readNextNonFillerKL(&key, &llen, &len);
        CHECK(HeaderMetadata::isHeaderMetadata(&key), "%s header partition holds no metadata", filename.c_str());

        HeaderMetadata header_metadata(&data_model);
        header_metadata.read(file, &file->getPartition(0), &key, llen, len);

        Preface *preface = header_metadata.getPreface();
        vector<mxfUL> ecs = preface->getEssenceContainers();
        for (size_t i = 0; i < ecs.size(); i++) {
            if (mxf_equals_ul_mod_regver(&ecs[i], &MXF_EC_L(VBIData)))
                info->have_vbi_ec = true;
            else if (mxf_equals_ul_mod_regver(&ecs[i], &MXF_EC_L(ANCData)))
                info->have_anc_ec = true;
        }

        vector<GenericPackage*> packages = preface->getContentStorage()->getPackages();
        for (size_t i = 0; i < packages.size(); i++) {
            SourcePackage *source_package = dynamic_cast<SourcePackage*>(packages[i]);
            bool is_file_package = source_package && source_package->haveDescriptor();
            bool is_material_package = dynamic_cast<MaterialPackage*>(packages[i]) != 0;

            vector<GenericTrack*> tracks = packages[i]->getTracks();
            for (size_t j = 0; j < tracks.size(); j++) {
                mxfUL data_def = tracks[j]->getSequence()->getDataDefinition();
                if (is_material_package && mxf_is_data(&data_def))
                    info->material_data_tracks++;
                if (is_file_package && tracks[j]->getTrackNumber() != 0) {
                    CHECK(info->file_track_id_by_number.count(tracks[j]->getTrackNumber()) == 0,
                          "track number 0x%08x is stated by two file package tracks", tracks[j]->getTrackNumber());
                    info->file_track_id_by_number[tracks[j]->getTrackNumber()] = tracks[j]->getTrackID();
                }
            }

            if (!is_file_package)
                continue;
            MultipleDescriptor *multiple = dynamic_cast<MultipleDescriptor*>(source_package->getDescriptor());
            CHECK(multiple, "the file package descriptor is not a multiple descriptor");
            vector<GenericDescriptor*> sub_descriptors = multiple->getSubDescriptorUIDs();
            for (size_t j = 0; j < sub_descriptors.size(); j++) {
                VBIDataDescriptor *vbi = dynamic_cast<VBIDataDescriptor*>(sub_descriptors[j]);
                ANCDataDescriptor *anc = dynamic_cast<ANCDataDescriptor*>(sub_descriptors[j]);
                if (vbi) {
                    CHECK(vbi->haveLinkedTrackID(), "the VBI data descriptor has no linked track id");
                    info->vbi_linked_track_ids.push_back(vbi->getLinkedTrackID());
                } else if (anc) {
                    CHECK(anc->haveLinkedTrackID(), "the ANC data descriptor has no linked track id");
                    info->anc_linked_track_ids.push_back(anc->getLinkedTrackID());
                }
            }
        }
    } catch (const MXFException &ex) {
        delete file;
        CHECK(false, "reading the header of %s: %s", filename.c_str(), ex.getMessage().c_str());
    } catch (...) {
        delete file;
        CHECK(false, "reading the header of %s: unexpected exception", filename.c_str());
    }

    delete file;
    return true;
}


struct BodyElement
{
    size_t key_offset;
    size_t value_offset;
    uint64_t len;
    mxfKey key;
    uint32_t track_number;
};

struct DeltaEntry
{
    int8_t pos_table_index;
    uint8_t slice;
    uint32_t element_delta;
};

struct IndexEntry
{
    uint64_t stream_offset;
    vector<uint32_t> slice_offsets;
};

struct IndexInfo
{
    uint32_t edit_unit_byte_count;
    vector<DeltaEntry> delta_entries;
    map<int64_t, IndexEntry> entries;
    int segments;
};

static bool parse_index_segment(const uint8_t *data, uint64_t len, IndexInfo *index)
{
    int64_t start_position = 0;
    uint8_t slice_count = 0;
    const uint8_t *delta_array = 0;
    const uint8_t *entry_array = 0;

    size_t pos = 0;
    while (pos + 4 <= len) {
        uint16_t tag = be16(&data[pos]);
        uint16_t tag_len = be16(&data[pos + 2]);
        const uint8_t *value = &data[pos + 4];
        CHECK(pos + 4 + tag_len <= len, "index segment local set overruns its KLV");
        switch (tag)
        {
            case 0x3f05: index->edit_unit_byte_count = be32(value); break;
            case 0x3f08: slice_count = value[0]; break;
            case 0x3f09: delta_array = value; break;
            case 0x3f0a: entry_array = value; break;
            case 0x3f0c: start_position = (int64_t)be64(value); break;
            default: break;
        }
        pos += 4 + tag_len;
    }

    if (delta_array) {
        uint32_t count = be32(delta_array);
        uint32_t size = be32(delta_array + 4);
        vector<DeltaEntry> delta_entries;
        for (uint32_t i = 0; i < count; i++) {
            const uint8_t *d = delta_array + 8 + i * size;
            DeltaEntry entry;
            entry.pos_table_index = (int8_t)d[0];
            entry.slice = d[1];
            entry.element_delta = be32(d + 2);
            delta_entries.push_back(entry);
        }
        CHECK(index->delta_entries.empty() || index->delta_entries.size() == delta_entries.size(),
              "index segments disagree on the delta entries");
        index->delta_entries = delta_entries;
    }

    if (entry_array) {
        uint32_t count = be32(entry_array);
        uint32_t size = be32(entry_array + 4);
        for (uint32_t i = 0; i < count; i++) {
            const uint8_t *e = entry_array + 8 + i * size;
            IndexEntry entry;
            entry.stream_offset = be64(e + 3);
            for (uint8_t s = 0; s < slice_count; s++)
                entry.slice_offsets.push_back(be32(e + 11 + 4 * s));
            index->entries[start_position + i] = entry;
        }
    }

    index->segments++;
    return true;
}

static bool walk_klvs(const string &filename, vector<BodyElement> *elements, IndexInfo *index)
{
    FILE *f = fopen(filename.c_str(), "rb");
    CHECK(f, "cannot open %s", filename.c_str());
    vector<uint8_t> bytes;
    unsigned char buffer[65536];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0)
        bytes.insert(bytes.end(), buffer, buffer + n);
    fclose(f);

    index->edit_unit_byte_count = 0;
    index->segments = 0;

    size_t pos = 0;
    while (pos + 17 <= bytes.size()) {
        mxfKey key;
        memcpy(&key, &bytes[pos], sizeof(key));

        uint8_t first = bytes[pos + 16];
        size_t llen = 1;
        uint64_t len = first;
        if (first & 0x80) {
            llen = 1 + (first & 0x7f);
            CHECK(llen <= 9 && pos + 16 + llen <= bytes.size(), "bad BER length at offset %u", (unsigned)pos);
            len = 0;
            for (size_t i = 1; i < llen; i++)
                len = (len << 8) | bytes[pos + 16 + i];
        }
        size_t value_offset = pos + 16 + llen;
        CHECK(value_offset + len <= bytes.size(), "KLV at offset %u overruns the file", (unsigned)pos);

        if (mxf_is_gc_essence_element(&key)) {
            BodyElement element;
            element.key_offset = pos;
            element.value_offset = value_offset;
            element.len = len;
            element.key = key;
            element.track_number = ((uint32_t)key.octet12 << 24) | ((uint32_t)key.octet13 << 16) |
                                   ((uint32_t)key.octet14 << 8)  |  (uint32_t)key.octet15;
            elements->push_back(element);
        } else if (mxf_is_index_table_segment(&key)) {
            if (!parse_index_segment(&bytes[value_offset], len, index))
                return false;
        }

        pos = value_offset + (size_t)len;
    }

    return true;
}


static bool verify_clip(const string &filename, const vector<EssenceType> &data_types, bool constant_size,
                        const map<EssenceType, uint32_t> &expected_track_numbers)
{
    HeaderInfo header;
    if (!read_header(filename, &header))
        return false;

    vector<BodyElement> elements;
    IndexInfo index;
    if (!walk_klvs(filename, &elements, &index))
        return false;

    const size_t elements_per_cp = 1 + data_types.size();

    // the header states one picture track and one track per data track
    CHECK(header.file_track_id_by_number.size() == elements_per_cp,
          "the file package states %u essence tracks, expected %u",
          (unsigned)header.file_track_id_by_number.size(), (unsigned)elements_per_cp);
    CHECK(header.material_data_tracks == data_types.size(),
          "the material package has %u data tracks, expected %u",
          header.material_data_tracks, (unsigned)data_types.size());

    // every body element's track number is one the header states
    for (size_t i = 0; i < elements.size(); i++) {
        CHECK(header.file_track_id_by_number.count(elements[i].track_number),
              "body element at offset %u has track number 0x%08x, which no header track states",
              (unsigned)elements[i].key_offset, elements[i].track_number);
    }

    // one element of each track per content package: the data tracks in the order added, then the picture
    CHECK(elements.size() == FRAMES * elements_per_cp,
          "the body has %u essence elements, expected %u", (unsigned)elements.size(),
          (unsigned)(FRAMES * elements_per_cp));

    vector<uint32_t> data_track_numbers;
    for (size_t j = 0; j < data_types.size(); j++) {
        uint32_t track_number = elements[j].track_number;
        CHECK(((track_number >> 24) & 0xff) == GC_DATA_ITEM_TYPE,
              "content package element %u (0x%08x) is not a GC data element", (unsigned)j, track_number);
        CHECK(((track_number >> 16) & 0xff) == data_types.size(),
              "data element 0x%08x states an element count of %u, expected %u", track_number,
              (track_number >> 16) & 0xff, (unsigned)data_types.size());
        CHECK(((track_number >> 8) & 0xff) == element_type_of(data_types[j]),
              "data element %u (0x%08x) is not the %s element", (unsigned)j, track_number,
              essence_type_to_string(data_types[j]));
        for (size_t k = 0; k < data_track_numbers.size(); k++)
            CHECK(data_track_numbers[k] != track_number, "two data tracks share track number 0x%08x", track_number);
        data_track_numbers.push_back(track_number);

        map<EssenceType, uint32_t>::const_iterator expected = expected_track_numbers.find(data_types[j]);
        if (expected != expected_track_numbers.end()) {
            CHECK(track_number == expected->second, "the %s track number is 0x%08x, expected 0x%08x",
                  essence_type_to_string(data_types[j]), track_number, expected->second);
        }
    }
    uint32_t picture_track_number = elements[data_types.size()].track_number;
    CHECK(((picture_track_number >> 24) & 0xff) == 0x15,
          "the last element of a content package (0x%08x) is not the picture", picture_track_number);

    vector<size_t> cp_starts;
    for (uint32_t frame = 0; frame < FRAMES; frame++) {
        const BodyElement *cp = &elements[frame * elements_per_cp];
        cp_starts.push_back(cp[0].key_offset);
        for (size_t j = 0; j < data_types.size(); j++) {
            CHECK(cp[j].track_number == data_track_numbers[j],
                  "content package %u element %u has track number 0x%08x, expected 0x%08x",
                  frame, (unsigned)j, cp[j].track_number, data_track_numbers[j]);
            vector<uint8_t> payload = st436_payload(data_types[j], frame);
            FILE *f = fopen(filename.c_str(), "rb");
            CHECK(f, "cannot reopen %s", filename.c_str());
            vector<uint8_t> value((size_t)cp[j].len + 1);
            fseek(f, (long)cp[j].value_offset, SEEK_SET);
            size_t read = fread(&value[0], 1, (size_t)cp[j].len, f);
            fclose(f);
            CHECK(read == cp[j].len && cp[j].len == payload.size() &&
                      memcmp(&value[0], &payload[0], payload.size()) == 0,
                  "content package %u: the %s element does not hold its own payload",
                  frame, essence_type_to_string(data_types[j]));
        }
        CHECK(cp[data_types.size()].track_number == picture_track_number,
              "content package %u does not end with the picture", frame);
    }

    // the descriptors are linked to the tracks carrying their elements
    size_t vbi_tracks = 0, anc_tracks = 0;
    for (size_t j = 0; j < data_types.size(); j++) {
        uint32_t track_id = header.file_track_id_by_number[data_track_numbers[j]];
        const vector<uint32_t> &linked = (data_types[j] == VBI_DATA ? header.vbi_linked_track_ids
                                                                    : header.anc_linked_track_ids);
        CHECK(linked.size() == 1, "%u %s descriptors, expected 1", (unsigned)linked.size(),
              essence_type_to_string(data_types[j]));
        CHECK(linked[0] == track_id, "the %s descriptor is linked to track %u, but its elements are track %u's",
              essence_type_to_string(data_types[j]), linked[0], track_id);
        if (data_types[j] == VBI_DATA)
            vbi_tracks++;
        else
            anc_tracks++;
    }
    CHECK(header.vbi_linked_track_ids.size() == vbi_tracks, "a VBI descriptor without a VBI track");
    CHECK(header.anc_linked_track_ids.size() == anc_tracks, "an ANC descriptor without an ANC track");
    CHECK(header.have_vbi_ec == (vbi_tracks > 0), "the VBI essence container is %slisted",
          header.have_vbi_ec ? "" : "not ");
    CHECK(header.have_anc_ec == (anc_tracks > 0), "the ANC essence container is %slisted",
          header.have_anc_ec ? "" : "not ");

    // the index table locates every element of every content package
    CHECK(index.segments > 0, "no index table segment");
    CHECK(index.delta_entries.size() == elements_per_cp, "%u delta entries, expected %u",
          (unsigned)index.delta_entries.size(), (unsigned)elements_per_cp);
    if (constant_size) {
        CHECK(index.edit_unit_byte_count == cp_starts[1] - cp_starts[0],
              "edit unit byte count %u, expected %u", index.edit_unit_byte_count,
              (unsigned)(cp_starts[1] - cp_starts[0]));
        for (uint32_t frame = 0; frame < FRAMES; frame++) {
            for (size_t j = 0; j < elements_per_cp; j++) {
                CHECK(index.delta_entries[j].slice == 0, "a CBE index states slice %u", index.delta_entries[j].slice);
                CHECK(cp_starts[frame] + index.delta_entries[j].element_delta ==
                          elements[frame * elements_per_cp + j].key_offset,
                      "the CBE index misplaces element %u of content package %u", (unsigned)j, frame);
            }
        }
    } else {
        CHECK(index.edit_unit_byte_count == 0, "a VBE index states an edit unit byte count");
        CHECK(index.entries.size() == FRAMES, "%u index entries, expected %u", (unsigned)index.entries.size(),
              FRAMES);
        const IndexEntry &first = index.entries[0];
        for (uint32_t frame = 0; frame < FRAMES; frame++) {
            CHECK(index.entries.count(frame), "no index entry for content package %u", frame);
            const IndexEntry &entry = index.entries[frame];
            CHECK(entry.stream_offset - first.stream_offset == cp_starts[frame] - cp_starts[0],
                  "the index misplaces content package %u", frame);
            for (size_t j = 0; j < elements_per_cp; j++) {
                uint8_t slice = index.delta_entries[j].slice;
                CHECK(slice <= entry.slice_offsets.size(), "delta entry %u names slice %u of %u", (unsigned)j, slice,
                      (unsigned)entry.slice_offsets.size());
                size_t slice_start = cp_starts[frame] + (slice > 0 ? entry.slice_offsets[slice - 1] : 0);
                CHECK(slice_start + index.delta_entries[j].element_delta ==
                          elements[frame * elements_per_cp + j].key_offset,
                      "the VBE index misplaces element %u of content package %u", (unsigned)j, frame);
            }
        }
    }

    return true;
}

// bmx's own reader finds each data track and hands every frame its own payload
static bool read_back_with_bmx(const string &filename, const vector<EssenceType> &data_types)
{
    MXFFileReader reader;
    CHECK(reader.Open(filename) == MXFFileReader::MXF_RESULT_SUCCESS, "MXFFileReader cannot open %s",
          filename.c_str());
    CHECK(reader.GetNumTrackReaders() == 1 + data_types.size(), "MXFFileReader finds %u tracks, expected %u",
          (unsigned)reader.GetNumTrackReaders(), (unsigned)(1 + data_types.size()));

    vector<MXFTrackReader*> data_readers;
    for (size_t j = 0; j < data_types.size(); j++) {
        MXFTrackReader *found = 0;
        for (size_t i = 0; i < reader.GetNumTrackReaders(); i++) {
            if (reader.GetTrackReader(i)->GetTrackInfo()->essence_type == data_types[j]) {
                CHECK(!found, "MXFFileReader finds two %s tracks", essence_type_to_string(data_types[j]));
                found = reader.GetTrackReader(i);
            }
        }
        CHECK(found, "MXFFileReader finds no %s track", essence_type_to_string(data_types[j]));
        data_readers.push_back(found);
    }

    for (uint32_t frame = 0; frame < FRAMES; frame++) {
        CHECK(reader.Read(1) == 1, "MXFFileReader cannot read frame %u", frame);
        for (size_t j = 0; j < data_readers.size(); j++) {
            Frame *read_frame = data_readers[j]->GetFrameBuffer()->GetLastFrame(true);
            CHECK(read_frame, "MXFFileReader gives no %s frame %u", essence_type_to_string(data_types[j]), frame);
            vector<uint8_t> payload = st436_payload(data_types[j], frame);
            bool same = read_frame->GetSize() == payload.size() &&
                        memcmp(read_frame->GetBytes(), &payload[0], payload.size()) == 0;
            delete read_frame;
            CHECK(same, "MXFFileReader gives %s frame %u another track's payload",
                  essence_type_to_string(data_types[j]), frame);
        }
    }

    return true;
}

static bool run_case(const char *name, const vector<EssenceType> &data_types, bool constant_size,
                     const map<EssenceType, uint32_t> &expected_track_numbers)
{
    string filename = string("test_op1a_data_tracks_") + name + ".mxf";
    bool ok = write_clip(filename, data_types, constant_size) &&
              verify_clip(filename, data_types, constant_size, expected_track_numbers) &&
              read_back_with_bmx(filename, data_types);
    if (ok)
        remove(filename.c_str()); // a failing clip is kept for inspection
    return ok;
}


static void test_single_anc_track_unchanged()
{
    TEST("a single ANC track keeps track number 0x17010201 and its key");

    vector<EssenceType> types(1, ANC_DATA);
    map<EssenceType, uint32_t> expected;
    expected[ANC_DATA] = 0x17010201;    // what the writer gave it before MPS-1297
    if (!run_case("anc", types, false, expected))
        return;
    PASS();
}

static void test_single_vbi_track_unchanged()
{
    TEST("a single VBI track keeps track number 0x17010101 and its key");

    vector<EssenceType> types(1, VBI_DATA);
    map<EssenceType, uint32_t> expected;
    expected[VBI_DATA] = 0x17010101;    // what the writer gave it before MPS-1297
    if (!run_case("vbi", types, false, expected))
        return;
    PASS();
}

static bool check_single_track_keys()
{
    // the key of a single data track is libMXF's constant: byte 14 count 1, byte 16 number 1
    vector<EssenceType> types(1, ANC_DATA);
    string filename = "test_op1a_data_tracks_anc_key.mxf";
    if (!write_clip(filename, types, true))
        return false;
    vector<BodyElement> elements;
    IndexInfo index;
    if (!walk_klvs(filename, &elements, &index))
        return false;
    CHECK(!elements.empty(), "no essence elements");
    CHECK(memcmp(&elements[0].key, &MXF_EE_K(ANCData), sizeof(mxfKey)) == 0,
          "the ANC element key is not MXF_EE_K(ANCData)");
    remove(filename.c_str());

    types[0] = VBI_DATA;
    filename = "test_op1a_data_tracks_vbi_key.mxf";
    elements.clear();
    if (!write_clip(filename, types, true) || !walk_klvs(filename, &elements, &index))
        return false;
    CHECK(!elements.empty(), "no essence elements");
    CHECK(memcmp(&elements[0].key, &MXF_EE_K(VBIData), sizeof(mxfKey)) == 0,
          "the VBI element key is not MXF_EE_K(VBIData)");
    remove(filename.c_str());
    return true;
}

static void test_single_track_keys_unchanged()
{
    TEST("a single data track's element key is MXF_EE_K(ANCData) / MXF_EE_K(VBIData)");
    if (check_single_track_keys())
        PASS();
}

static void test_vbi_then_anc_tracks()
{
    TEST("VBI then ANC: 0x17020101 and 0x17020202, one element each per content package");

    vector<EssenceType> types;
    types.push_back(VBI_DATA);
    types.push_back(ANC_DATA);
    map<EssenceType, uint32_t> expected;
    expected[VBI_DATA] = 0x17020101;
    expected[ANC_DATA] = 0x17020202;
    if (!run_case("vbi_anc", types, false, expected))
        return;
    PASS();
}

static void test_anc_then_vbi_tracks()
{
    TEST("ANC then VBI: 0x17020201 and 0x17020102, one element each per content package");

    vector<EssenceType> types;
    types.push_back(ANC_DATA);
    types.push_back(VBI_DATA);
    map<EssenceType, uint32_t> expected;
    expected[ANC_DATA] = 0x17020201;
    expected[VBI_DATA] = 0x17020102;
    if (!run_case("anc_vbi", types, false, expected))
        return;
    PASS();
}

static void test_two_data_tracks_cbe_index()
{
    TEST("ANC and VBI of a constant size: the CBE index locates both elements");

    vector<EssenceType> types;
    types.push_back(ANC_DATA);
    types.push_back(VBI_DATA);
    map<EssenceType, uint32_t> expected;
    expected[ANC_DATA] = 0x17020201;
    expected[VBI_DATA] = 0x17020102;
    if (!run_case("anc_vbi_cbe", types, true, expected))
        return;
    PASS();
}

static bool check_rdd9_refuses_second_data_track()
{
    string filename = "test_op1a_data_tracks_rdd9.mxf";
    bool refused = false;
    string message;
    try {
        RDD9File clip(0, File::openNew(filename), FRAME_RATE);
        clip.CreateTrack(ANC_DATA);
        try {
            clip.CreateTrack(VBI_DATA);
        } catch (const BMXException &ex) {
            refused = true;
            message = ex.what();
        }
    } catch (const BMXException &ex) {
        remove(filename.c_str());
        CHECK(false, "RDD 9 clip: %s", ex.what());
    } catch (...) {
        remove(filename.c_str());
        CHECK(false, "RDD 9 clip: unexpected exception");
    }
    remove(filename.c_str());

    CHECK(refused, "RDD 9 accepted a second data track, which its writer cannot number");
    CHECK(message.find("RDD 9") != string::npos, "the refusal does not name RDD 9: '%s'", message.c_str());
    return true;
}

static void test_rdd9_refuses_second_data_track()
{
    TEST("RDD 9 refuses a second ST 436 data track when it is added");
    if (check_rdd9_refuses_second_data_track())
        PASS();
}


int main(int argc, const char **argv)
{
    (void)argc;
    (void)argv;

    printf("OP1a ST 436 data track tests (MPS-1297)\n\n");

    test_single_anc_track_unchanged();
    test_single_vbi_track_unchanged();
    test_single_track_keys_unchanged();
    test_vbi_then_anc_tracks();
    test_anc_then_vbi_tracks();
    test_two_data_tracks_cbe_index();
    test_rdd9_refuses_second_data_track();

    printf("\n%d/%d tests passed\n", pass_count, test_count);
    return pass_count == test_count ? 0 : 1;
}
