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
typedef std::vector<std::vector<float>> HeightField;
typedef std::vector<std::vector<std::uint32_t>> TriStrips;
#define IO_VERTEXFIELDOUT_TYPE VertexFieldOut_Template<HeightField,TriStrips>
#include "vertexfield_io.hpp"
#include <vector>
#include <iostream>
#include <cmath>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>


static void minmax(io::VertexFieldIn::mapType& Map, float& Min, float& Max) {
    Min = Max = Map.front().front();
    for (auto& line : Map)
        for (auto& value : line)
            if (value < Min)
                Min = value;
            else if (Max < value)
                Max = value;
}

static void create_output(
    io::VertexFieldOut& Out, io::VertexFieldIn& Val, float Min, float Max)
{
    // Each location into a vertex.
    Out.vertices.reserve(Val.map().size() * Val.map().front().size());
    float yscale = (Val.width() * Val.map().size()) / Val.map().front().size();
    const float zscale = Val.range() / (Max - Min);
    for (size_t y = 0; y < Val.map().size(); ++y) {
        const float yc = (y * yscale) / (Val.map().size() - 1);
        for (size_t x = 0; x < Val.map()[y].size(); ++x) {
            float xc = (x * Val.width()) / (Val.map().front().size() - 1);
            float zc = zscale * Val.map()[y][x];
            Out.vertices.push_back(std::vector<float> { xc, yc, zc });
        }
    }
    // Each row into triangle strip using the indexes of the next row, too.
    // Last row is skipped.
    Out.tristrips.reserve(Val.map().size() - 1);
    const std::uint32_t step = Val.map().front().size();
    for (std::uint32_t y = 0; y < Val.map().size() - 1; ++y) {
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
    io::VertexFieldIn_Parser parser;
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
        io::VertexFieldIn val;
        parser.Swap(val.values);
        io::VertexFieldOut out;
        if (!val.map().empty() && !val.map().front().empty()) {
            if (!val.widthGiven())
                val.width() = val.map().front().size() - 1;
            float min, max;
            minmax(val.map(), min, max);
            if (!val.rangeGiven())
                val.range() = max - min;
            create_output(out, val, min, max);
        }
        Write(std::cout, out, output_buffer);
        std::cout << std::endl;
    }
    if (f)
        close(f);
    return 0;
}

#else

TEST_CASE("minmax") {
    io::VertexFieldIn::mapType map;
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
    io::VertexFieldOut out;
    io::VertexFieldIn val;
    val.range() = 4.0f;
    val.width() = 8.0f;
    float min, max;
    SUBCASE("1 strip") {
        val.map().resize(0);
        val.map().push_back(std::vector<float> { -1.0f, 0.0f });
        val.map().push_back(std::vector<float> { 1.0f, 0.5f });
        minmax(val.map(), min, max);
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
        val.map().resize(0);
        val.map().push_back(std::vector<float> { -1.0f, 0.0f });
        val.map().push_back(std::vector<float> { 3.0f, 2.0f });
        val.map().push_back(std::vector<float> { 1.0f, 0.5f });
        minmax(val.map(), min, max);
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
