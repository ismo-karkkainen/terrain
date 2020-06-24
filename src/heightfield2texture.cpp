//
//  heightfield2texture.cpp
//
//  Created by Ismo Kärkkäinen on 23.6.2020.
//  Copyright © 2020 Ismo Kärkkäinen. All rights reserved.
//
// Licensed under Universal Permissive License. See License.txt.

#if defined(UNITTEST)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#else
#include "convenience.hpp"
#endif
#include <vector>
typedef std::vector<std::vector<std::vector<float>>> Texture;
typedef std::vector<std::vector<float>> Coords;
#define IO_HEIGHTFIELD2TEXTUREOUT_TYPE HeightField2TextureOut_Template<Texture,Coords>
#include "heightfield2texture_io.hpp"
#include "colormap.hpp"
#include <iostream>
#include <cmath>
#include <fcntl.h>
#include <unistd.h>


static void texture(
    io::HeightField2TextureOut& Out, io::HeightField2TextureIn& Val)
{
    std::vector<std::vector<float>> map = Val.colormap();
    SortColorMap(map);
    // Texture is a color map with each discontinuity mapped to a texel.
    Out.texture.push_back(std::vector<std::vector<float>>());
    Out.texture.back().reserve(map.size());
    for (auto& limit : map) {
        Out.texture.back().push_back(std::vector<float>(limit.size() - 1, 0.0f));
        for (size_t k = 1; k < limit.size(); ++k)
            Out.texture.back().back()[k - 1] = limit[k];
    }
}

static void coordinates(
    io::HeightField2TextureOut& Out, io::HeightField2TextureIn& Val)
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
    Out.coordinates.reserve(
        Val.heightfield().size() * Val.heightfield().front().size());
    for (auto& line : Val.heightfield())
        for (auto& value : line) {
            float v = (value - min) / range;
            size_t idx = IndexInMap(v, map);
            float s;
            if (idx == 0 && v <= map.front().front())
                s = 0.0f;
            else if (idx == map.size() - 1)
                s = 1.0f;
            else // idx + relative location of v in range, mapped to [0, 1].
                s = (idx
                    + (v - map[idx].front())
                        / (map[idx + 1].front() - map[idx].front()))
                    / (map.size() - 1);
            Out.coordinates.push_back(std::vector<float> { s, 0.5f });
        }
}

#if !defined(UNITTEST)

static int texcoord(io::HeightField2TextureIn& Val) {
    std::vector<char> output_buffer;
    io::HeightField2TextureOut out;
    texture(out, Val);
    coordinates(out, Val);
    Write(std::cout, out, output_buffer);
    std::cout << std::endl;
    return 0;
}

int main(int argc, char** argv) {
    int f = 0;
    if (argc > 1)
        f = open(argv[1], O_RDONLY);
    InputParser<io::ParserPool, io::HeightField2TextureIn_Parser,
        io::HeightField2TextureIn> ip(f);
    int status = ip.ReadAndParse(texcoord);
    if (f)
        close(f);
    return status;
}

#else

TEST_CASE("texture") {
    io::HeightField2TextureOut out;
    io::HeightField2TextureIn val;
    SUBCASE("min < max") {
        val.heightfield().resize(0);
        val.colormap().resize(0);
        out.texture.resize(0);
        val.heightfield().push_back(std::vector<float> { -1.0f, 0.0f });
        val.heightfield().push_back(std::vector<float> { 1.0f, 0.5f });
        val.colormap().push_back(std::vector<float> { 0.0f, 0.0f, 0.0f, 0.0f });
        val.colormap().push_back(std::vector<float> { 1.0f, 1.0f, 0.5f, 0.0f });
        texture(out, val);
        REQUIRE(out.texture.size() == 1);
        REQUIRE(out.texture.front().size() == val.colormap().size());
        REQUIRE(out.texture[0][0].size() == val.colormap().front().size() - 1);
        REQUIRE(out.texture[0][0] == std::vector<float> { 0.0f, 0.0f, 0.0f });
        REQUIRE(out.texture[0][1] == std::vector<float> { 1.0f, 0.5f, 0.0f });
    }
    SUBCASE("min == max") {
        val.heightfield().resize(0);
        val.colormap().resize(0);
        out.texture.resize(0);
        val.heightfield().push_back(std::vector<float> { 1.0f, 1.0f });
        val.heightfield().push_back(std::vector<float> { 1.0f, 1.0f });
        val.colormap().push_back(std::vector<float> { 0.0f, 0.0f, 0.0f, 0.0f });
        val.colormap().push_back(std::vector<float> { 1.0f, 1.0f, 0.5f, 0.0f });
        texture(out, val);
        REQUIRE(out.texture[0][0] == std::vector<float> { 0.0f, 0.0f, 0.0f });
    }
}

TEST_CASE("coordinates") {
    io::HeightField2TextureOut out;
    io::HeightField2TextureIn val;
    SUBCASE("min < max") {
        val.heightfield().resize(0);
        val.colormap().resize(0);
        out.coordinates.resize(0);
        val.heightfield().push_back(std::vector<float> { -1.0f, 0.0f });
        val.heightfield().push_back(std::vector<float> { 1.0f, 0.5f });
        val.colormap().push_back(std::vector<float> { 0.0f, 0.0f, 0.0f, 0.0f });
        val.colormap().push_back(std::vector<float> { 1.0f, 1.0f, 0.5f, 0.0f });
        coordinates(out, val);
        REQUIRE(out.coordinates.size() == val.heightfield().size() * val.heightfield().front().size());
        for (size_t k = 0; k < out.coordinates.size(); ++k)
            REQUIRE(out.coordinates[k].size() == 2);
        REQUIRE(out.coordinates[0] == std::vector<float> { 0.0f, 0.5f });
        REQUIRE(out.coordinates[2] == std::vector<float> { 1.0f, 0.5f });
    }
    SUBCASE("min == max") {
        val.heightfield().resize(0);
        val.colormap().resize(0);
        out.coordinates.resize(0);
        val.heightfield().push_back(std::vector<float> { 1.0f, 1.0f });
        val.heightfield().push_back(std::vector<float> { 1.0f, 1.0f });
        val.colormap().push_back(std::vector<float> { 0.0f, 0.0f, 0.0f, 0.0f });
        val.colormap().push_back(std::vector<float> { 1.0f, 1.0f, 0.5f, 0.0f });
        coordinates(out, val);
        REQUIRE(out.coordinates[0] == std::vector<float> { 0.0f, 0.5f });
    }
}

#endif
