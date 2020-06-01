//
//  pseudocolor.cpp
//
//  Created by Ismo Kärkkäinen on 1.6.2020.
//  Copyright © 2020 Ismo Kärkkäinen. All rights reserved.
//
// Licensed under Universal Permissive License. See License.txt.

#if defined(UNITTEST)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#else
#include "FileDescriptorInput.hpp"
#include "BlockQueue.hpp"
#endif
typedef std::vector<std::vector<std::vector<float>>> Image;
#define IO_PSEUDOCOLOROUT_TYPE PseudoColorOut_Template<Image>
#include "color_io.hpp"
#include <vector>
#include <iostream>
#include <cmath>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>

static std::vector<float> color(float v, const float water) {
    float r, g, b;
    if (v < water) {
        v /= water;
        r = 0.0f;
        g = 3.0f * v - 2.0f;
        if (g < 0.0f)
            g = 0.0f;
        b = v;
    } else if (water < v) {
        v = (v - water) / (1.0f - water);
        r = 2.0f * v - 1.0f;
        if (r < 0.0f)
            r = 0.0f;
        g = 1.0f - v;
        if (g < 0.5f)
            g = 0.5f;
        b = 0.0f;
    } else {
        r = 0.0f;
        g = 1.0f;
        b = 0.5f;
    }
    return std::vector<float> { r, g, b };
}

static void color_map(io::PseudoColorOut& Out, io::PseudoColorIn& Val) {
    float min, max;
    min = max = Val.map()[0][0];
    for (auto& line : Val.map())
        for (auto& value : line) {
            if (value < min)
                min = value;
            if (max < value)
                max = value;
        }
    Out.image.reserve(Val.map().size());
    const float water = Val.waterlevel();
    const float range = (min < max) ? max - min : 1.0f;
    for (auto& line : Val.map()) {
        Out.image.push_back(std::vector<std::vector<float>>());
        Out.image.back().reserve(line.size());
        for (auto& value : line)
            Out.image.back().push_back(color((value - min) / range, water));
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
    io::PseudoColorIn_Parser parser;
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
        io::PseudoColorIn val;
        parser.Swap(val.values);
        if (!val.waterlevelGiven())
            val.waterlevel() = 0.5f;
        io::PseudoColorOut out;
        color_map(out, val);
        Write(std::cout, out, output_buffer);
        std::cout << std::endl;
    }
    if (f)
        close(f);
    return 0;
}

#else

TEST_CASE("color") {
    SUBCASE("zero") {
        const std::vector<float> border = { 0.0f, 0.0f, 0.0f };
        REQUIRE(color(0.0f, 0.25f) == border);
        REQUIRE(color(0.0f, 0.5f) == border);
        REQUIRE(color(0.0f, 0.75f) == border);
        REQUIRE(color(0.0f, 1.0f) == border);
    }
    SUBCASE("water") {
        const std::vector<float> border = { 0.0f, 1.0f, 0.5f };
        REQUIRE(color(0.0f, 0.0f) == border);
        REQUIRE(color(0.25f, 0.25f) == border);
        REQUIRE(color(0.5f, 0.5f) == border);
        REQUIRE(color(0.75f, 0.75f) == border);
        REQUIRE(color(1.0f, 1.0f) == border);
    }
    SUBCASE("one") {
        const std::vector<float> border = { 1.0f, 0.5f, 0.0f };
        REQUIRE(color(1.0f, 0.0f) == border);
        REQUIRE(color(1.0f, 0.25f) == border);
        REQUIRE(color(1.0f, 0.5f) == border);
        REQUIRE(color(1.0f, 0.75f) == border);
    }
}

TEST_CASE("color_map") {
    io::PseudoColorOut out;
    io::PseudoColorIn val;
    SUBCASE("min < max") {
        val.map().push_back(std::vector<float> { -1.0f, 0.0f });
        val.map().push_back(std::vector<float> { 1.0f, 0.5f });
        val.waterlevel() = 0.75;
        color_map(out, val);
        REQUIRE(out.image.size() == val.map().size());
        for (size_t k = 0; k < out.image.size(); ++k)
            REQUIRE(out.image[k].size() == val.map()[k].size());
        REQUIRE(out.image[0][0].size() == 3);
        REQUIRE(out.image[0][0] == std::vector<float> { 0.0f, 0.0f, 0.0f });
        REQUIRE(out.image[1][0] == std::vector<float> { 1.0f, 0.5f, 0.0f });
        val.map().resize(0);
        out.image.resize(0);
    }
    SUBCASE("min == max") {
        val.map().push_back(std::vector<float> { 1.0f, 1.0f });
        val.map().push_back(std::vector<float> { 1.0f, 1.0f });
        val.waterlevel() = 0.75;
        color_map(out, val);
        REQUIRE(out.image[0][0] == std::vector<float> { 0.0f, 0.0f, 0.0f });
        val.map().resize(0);
        out.image.resize(0);
    }
}

#endif
