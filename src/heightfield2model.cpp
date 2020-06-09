//
//  vertexfield.cpp
//
//  Created by Ismo Kärkkäinen on 8.6.2020.
//  Copyright © 2020 Ismo Kärkkäinen. All rights reserved.
//
// Licensed under Universal Permissive License. See License.txt.

#if defined(UNITTEST)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <algorithm>
#else
#include "FileDescriptorInput.hpp"
#include "BlockQueue.hpp"
#endif
#include <cinttypes>
typedef std::vector<std::vector<float>> V3;
typedef std::vector<std::vector<std::uint32_t>> TriStrips;
#define IO_HEIGHTFIELD2MODELOUT_TYPE HeightField2ModelOut_Template<V3,V3,TriStrips>
#include "heightfield2model_io.hpp"
#include "colormap.hpp"
#include <vector>
#include <iostream>
#include <cmath>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>


static void minmax(
    io::HeightField2ModelIn::heightfieldType& Map, float& Min, float& Max)
{
    Min = Max = Map.front().front();
    for (auto& line : Map)
        for (auto& value : line)
            if (value < Min)
                Min = value;
            else if (Max < value)
                Max = value;
}

static void create_output(io::HeightField2ModelOut& Out,
    io::HeightField2ModelIn& Val, float Min, float Max)
{
    // Each location into a vertex.
    Out.vertices.reserve(Val.heightfield().size() * Val.heightfield().front().size());
    float yscale = (Val.width() * Val.heightfield().size()) / Val.heightfield().front().size();
    const float zscale = Val.range() / (Max - Min);
    for (size_t y = 0; y < Val.heightfield().size(); ++y) {
        const float yc = (y * yscale) / (Val.heightfield().size() - 1);
        for (size_t x = 0; x < Val.heightfield()[y].size(); ++x) {
            float xc = (x * Val.width()) / (Val.heightfield().front().size() - 1);
            float zc = zscale * Val.heightfield()[y][x];
            Out.vertices.push_back(std::vector<float> { xc, yc, zc });
        }
    }
    // Each row into triangle strip using the indexes of the next row, too.
    // Last row is skipped.
    Out.tristrips.reserve(Val.heightfield().size() - 1);
    const std::uint32_t step = Val.heightfield().front().size();
    for (std::uint32_t y = 0; y < Val.heightfield().size() - 1; ++y) {
        Out.tristrips.push_back(std::vector<std::uint32_t>());
        Out.tristrips.back().reserve(2 * step);
        for (std::uint32_t x = 0; x < step; ++x)
            if (y & 1) { // Makes zig-zag and not straight triangle edge lines.
                Out.tristrips.back().push_back(y * step + x);
                Out.tristrips.back().push_back((y + 1) * step + x);
            } else {
                Out.tristrips.back().push_back((y + 1) * step + x);
                Out.tristrips.back().push_back(y * step + x);
            }
    }
    if (Val.colormapGiven()) {
        const float range = (Min < Max) ? Max - Min : 1.0f;
        std::vector<std::vector<float>> map = Val.colormap();
        SortColorMap(map);
        Out.colors.reserve(Out.vertices.size());
        for (auto& line : Val.heightfield())
            for (auto& v : line)
                Out.colors.push_back(Interpolated((v - Min) / range, map));
    }
}

#if !defined(UNITTEST)
static const size_t block_size = 1048576;

int main(int argc, char** argv) {
    int f = 0;
    if (argc > 1)
        f = open(argv[1], O_RDONLY);
    FileDescriptorInput input(f);
    BlockQueue::BlockPtr buffer;
    io::ParserPool pp;
    io::HeightField2ModelIn_Parser parser;
    std::vector<char> output_buffer;
    const char* end = nullptr;
    while (!input.Ended()) {
        if (end == nullptr) {
            if (!buffer)
                buffer.reset(new BlockQueue::Block());
            if (buffer->size() != block_size + 1)
                buffer->resize(block_size + 1);
            int count = input.Read(&buffer->front(), block_size);
            if (count == 0) {
                struct timespec ts;
                ts.tv_sec = 0;
                ts.tv_nsec = 100000000;
                nanosleep(&ts, nullptr);
                continue;
            }
            buffer->resize(count + 1);
            buffer->back() = 0;
            end = &buffer->front();
        }
        if (parser.Finished()) {
            end = pp.skipWhitespace(end, &buffer->back());
            if (end == nullptr)
                continue;
        }
        try {
            end = parser.Parse(end, &buffer->back(), pp);
        }
        catch (const io::Exception& e) {
            std::cerr << e.what() << std::endl;
            return 1;
        }
        if (!parser.Finished()) {
            end = nullptr;
            continue;
        }
        io::HeightField2ModelIn val;
        parser.Swap(val.values);
        if (val.heightfield().size() < 2) {
            std::cerr << "Height field has less than 2 rows." << std::endl;
            return 1;
        }
        if (val.heightfield().front().size() < 2) {
            std::cerr << "Height field has less than 2 columns." << std::endl;
            return 1;
        }
        if (!val.widthGiven())
            val.width() = val.heightfield().front().size() - 1;
        float min, max;
        minmax(val.heightfield(), min, max);
        if (!val.rangeGiven())
            val.range() = max - min;
        io::HeightField2ModelOut out;
        create_output(out, val, min, max);
        Write(std::cout, out, output_buffer);
        std::cout << std::endl;
    }
    if (f)
        close(f);
    return 0;
}

#else

TEST_CASE("minmax") {
    io::HeightField2ModelIn::heightfieldType map;
    SUBCASE("min first") {
        map.resize(0);
        map.push_back(std::vector<float> { -1.0f, 0.0f });
        map.push_back(std::vector<float> { 1.0f, 0.5f });
        float min, max;
        minmax(map, min, max);
        REQUIRE(min == -1.0f);
        REQUIRE(max == 1.0f);
    }
    SUBCASE("max first") {
        map.resize(0);
        map.push_back(std::vector<float> { 2.0f, 0.0f });
        map.push_back(std::vector<float> { 1.0f, 0.5f });
        float min, max;
        minmax(map, min, max);
        REQUIRE(min == 0.0f);
        REQUIRE(max == 2.0f);
    }
    SUBCASE("min and max later") {
        map.resize(0);
        map.push_back(std::vector<float> { 2.0f, 3.0f });
        map.push_back(std::vector<float> { 1.0f, 0.5f });
        float min, max;
        minmax(map, min, max);
        REQUIRE(min == 0.5f);
        REQUIRE(max == 3.0f);
    }
}

TEST_CASE("create_output") {
    io::HeightField2ModelOut out;
    io::HeightField2ModelIn val;
    val.range() = 4.0f;
    val.width() = 8.0f;
    float min, max;
    SUBCASE("1 strip") {
        val.heightfield().resize(0);
        val.heightfield().push_back(std::vector<float> { -1.0f, 0.0f });
        val.heightfield().push_back(std::vector<float> { 1.0f, 0.5f });
        minmax(val.heightfield(), min, max);
        create_output(out, val, min, max);
        REQUIRE(out.vertices.size() == 4);
        REQUIRE(out.vertices[0].size() == 3);
        REQUIRE(out.vertices[0][0] == 0.0f);
        REQUIRE(out.vertices[0][1] == 0.0f);
        REQUIRE(out.vertices[0][2] == -2.0f);
        REQUIRE(out.vertices[1][0] == 8.0f);
        REQUIRE(out.vertices[1][1] == 0.0f);
        REQUIRE(out.vertices[1][2] == 0.0f);
        REQUIRE(out.vertices[2][0] == 0.0f);
        REQUIRE(out.vertices[2][1] == 8.0f);
        REQUIRE(out.vertices[2][2] == 2.0f);
        REQUIRE(out.tristrips.size() == 1);
        REQUIRE(out.tristrips[0].size() == 4);
        std::sort(out.tristrips[0].begin(), out.tristrips[0].end());
        for (std::uint32_t k = 0; k < out.tristrips[0].size(); ++k)
            REQUIRE(out.tristrips[0][k] == k);
    }
    SUBCASE("2 strips") {
        val.heightfield().resize(0);
        val.heightfield().push_back(std::vector<float> { -1.0f, 0.0f });
        val.heightfield().push_back(std::vector<float> { 3.0f, 2.0f });
        val.heightfield().push_back(std::vector<float> { 1.0f, 0.5f });
        minmax(val.heightfield(), min, max);
        create_output(out, val, min, max);
        REQUIRE(out.vertices.size() == 6);
        REQUIRE(out.tristrips.size() == 2);
        REQUIRE(out.tristrips[0].size() == 4);
        REQUIRE(out.tristrips[1].size() == 4);
        std::sort(out.tristrips[1].begin(), out.tristrips[1].end());
        for (std::uint32_t k = 0; k < out.tristrips[1].size(); ++k)
            REQUIRE(out.tristrips[1][k] == k + 2);
    }
}

#endif
