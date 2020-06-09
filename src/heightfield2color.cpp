//
//  heightfield2color.cpp
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
#define IO_HEIGHTFIELD2COLOROUT_TYPE HeightField2ColorOut_Template<Image>
#include "heightfield2color_io.hpp"
#include "colormap.hpp"
#include <vector>
#include <iostream>
#include <cmath>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>


static void color_map(
    io::HeightField2ColorOut& Out, io::HeightField2ColorIn& Val)
{
    float min, max;
    min = max = Val.heightfield()[0][0];
    for (auto& line : Val.heightfield())
        for (auto& value : line)
            if (value < min)
                min = value;
            else if (max < value)
                max = value;
    const float range = (min < max) ? max - min : 1.0f;
    std::vector<std::vector<float>> map = Val.colormap();
    SortColorMap(map);
    Out.image.reserve(Val.heightfield().size());
    for (auto& line : Val.heightfield()) {
        Out.image.push_back(std::vector<std::vector<float>>());
        Out.image.back().reserve(line.size());
        for (auto& v : line)
            Out.image.back().push_back(Interpolated((v - min) / range, map));
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
    io::HeightField2ColorIn_Parser parser;
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
        io::HeightField2ColorIn val;
        parser.Swap(val.values);
        io::HeightField2ColorOut out;
        color_map(out, val);
        Write(std::cout, out, output_buffer);
        std::cout << std::endl;
    }
    if (f)
        close(f);
    return 0;
}

#else

TEST_CASE("color_map") {
    io::HeightField2ColorOut out;
    io::HeightField2ColorIn val;
    SUBCASE("min < max") {
        val.heightfield().resize(0);
        out.image.resize(0);
        val.heightfield().push_back(std::vector<float> { -1.0f, 0.0f });
        val.heightfield().push_back(std::vector<float> { 1.0f, 0.5f });
        val.colormap().push_back(std::vector<float> { 0.0f, 0.0f, 0.0f, 0.0f });
        val.colormap().push_back(std::vector<float> { 1.0f, 1.0f, 0.5f, 0.0f });
        color_map(out, val);
        REQUIRE(out.image.size() == val.heightfield().size());
        for (size_t k = 0; k < out.image.size(); ++k)
            REQUIRE(out.image[k].size() == val.heightfield()[k].size());
        REQUIRE(out.image[0][0].size() == 3);
        REQUIRE(out.image[0][0] == std::vector<float> { 0.0f, 0.0f, 0.0f });
        REQUIRE(out.image[1][0] == std::vector<float> { 1.0f, 0.5f, 0.0f });
    }
    SUBCASE("min == max") {
        val.heightfield().resize(0);
        out.image.resize(0);
        val.heightfield().push_back(std::vector<float> { 1.0f, 1.0f });
        val.heightfield().push_back(std::vector<float> { 1.0f, 1.0f });
        val.colormap().push_back(std::vector<float> { 0.0f, 0.0f, 0.0f, 0.0f });
        val.colormap().push_back(std::vector<float> { 1.0f, 1.0f, 0.5f, 0.0f });
        color_map(out, val);
        REQUIRE(out.image[0][0] == std::vector<float> { 0.0f, 0.0f, 0.0f });
    }
}

#endif
