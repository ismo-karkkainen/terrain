//
//  generatechanges.cpp
//
//  Created by Ismo Kärkkäinen on 29.5.2020.
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
#include "generate_io.hpp"
#include <vector>
#include <iostream>
#include <cmath>
#include <ctime>
#include <cinttypes>
#include <functional>
#include <random>
#include <fcntl.h>
#include <unistd.h>


static std::mt19937 rnd;

typedef std::function<float(float,float,float)> Mapper;

static float value(float x, float y, io::GenerateIn::radius_minType& Map) {
    y *= Map.size() - 2;
    size_t yidx = trunc(y);
    y -= yidx;
    float xx = x * (Map[yidx].size() - 2);
    size_t xidx = trunc(xx);
    xx -= xidx;
    float v = (1.0f - y) * ((1.0f - xx) * Map[yidx][xidx] + xx * Map[yidx][xidx + 1]);
    ++yidx;
    xx = x * (Map[yidx].size() - 2);
    xidx = trunc(xx);
    xx -= xidx;
    return v + y * ((1.0f - xx) * Map[yidx][xidx] + xx * Map[yidx][xidx + 1]);
}

static void generate_change(
    std::vector<float>& Change, Mapper Radius, Mapper Offset)
{
    Change[2] = Radius(Change[0], Change[1], Change[2]);
    Change[3] = Offset(Change[0], Change[1], Change[3]);
}

static float minmax(float x, float y, float r,
    io::GenerateIn::radius_minType& a, io::GenerateIn::radius_minType& b)
{
    float v = value(x, y, a);
    return v + (value(x, y, b) - v) * r;
}

static float minrange(float x, float y, float r,
    io::GenerateIn::radius_minType& a, io::GenerateIn::radius_minType& b)
{
    return value(x, y, a) + value(x, y, b) * r;
}

static float maxrange(float x, float y, float r,
    io::GenerateIn::radius_minType& a, io::GenerateIn::radius_minType& b)
{
    return value(x, y, a) - value(x, y, b) * r;
}

static void check_map(
    bool Given, io::GenerateIn::radius_minType& Value, float Default)
{
    if (Given) {
        if (Value.empty())
            throw "Input map is empty.";
        for (auto& row : Value)
            if (row.empty())
                throw "Input map row is empty.";
    } else
        Value.push_back(std::vector<float>(1, Default));
    for (auto& row : Value)
        row.push_back(row.front());
    Value.push_back(Value.front());
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
    io::GenerateIn_Parser parser;
    std::vector<char> output_buffer;
    const float s = 1.0f / static_cast<float>(std::mt19937::max());
    std::vector<float> change(4, 0.0f);
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
        io::GenerateIn val;
        parser.Swap(val.values);
        try {
            check_map(val.radius_minGiven(), val.radius_min(), 0.0f);
            check_map(val.radius_maxGiven(), val.radius_max(), 1.0f);
            check_map(val.radius_rangeGiven(), val.radius_range(), 1.0f);
            check_map(val.offset_minGiven(), val.offset_min(), -1.0f);
            check_map(val.offset_maxGiven(), val.offset_max(), 1.0f);
            check_map(val.offset_rangeGiven(), val.offset_range(), 2.0f);
        }
        catch (const char* msg) {
            std::cerr << msg << std::endl;
            return 2;
        }
        if (val.seedGiven())
            rnd.seed(val.seed());
        Mapper radius = [&val](float x, float y, float r) {
            return minrange(x, y, r, val.radius_min(), val.radius_range());
        };
        if (val.radius_minGiven() && val.radius_maxGiven()) {
            radius = [&val](float x, float y, float r) {
                return minmax(x, y, r, val.radius_min(), val.radius_max());
            };
        } else if (val.radius_maxGiven()) {
            radius = [&val](float x, float y, float r) {
                return maxrange(x, y, r, val.radius_max(), val.radius_range());
            };
        }
        Mapper offset = [&val](float x, float y, float r) {
            return minrange(x, y, r, val.offset_min(), val.offset_range());
        };
        if (val.offset_minGiven() && val.offset_maxGiven()) {
            offset = [&val](float x, float y, float r) {
                return minmax(x, y, r, val.offset_min(), val.offset_max());
            };
        } else if (val.offset_maxGiven()) {
            offset = [&val](float x, float y, float r) {
                return maxrange(x, y, r, val.offset_max(), val.offset_range());
            };
        }
        std::cout << "{\"changes\":[";
        for (std::uint32_t k = 0; k < val.count(); ++k) {
            for (auto& v : change)
                v = s * static_cast<float>(rnd());
            generate_change(change, radius, offset);
            io::Write(std::cout, change, output_buffer);
            if (k + 1 != val.count())
                std::cout << ',';
        }
        std::cout << "]}" << std::endl;
    }
    if (f)
        close(f);
    return 0;
}

#else

TEST_CASE("check_map") {
    SUBCASE("Default filled.") {
        io::GenerateIn::radius_minType map;
        check_map(false, map, 0.5f);
        REQUIRE(map.size() == 2);
        for (auto& row : map) {
            REQUIRE(row.size() == 2);
            for (auto& val : row)
                REQUIRE(val == 0.5f);
        }
    }
    SUBCASE("Given expanded") {
        io::GenerateIn::radius_minType map;
        map.push_back(std::vector<float>(1, 0.5f));
        check_map(true, map, 0.25f);
        REQUIRE(map.size() == 2);
        for (auto& row : map) {
            REQUIRE(row.size() == 2);
            for (auto& val : row)
                REQUIRE(val == 0.5f);
        }
    }
    SUBCASE("Empty") {
        io::GenerateIn::radius_minType map;
        REQUIRE_THROWS_AS(check_map(true, map, 0.0f), const char*);
    }
    SUBCASE("Empty row") {
        io::GenerateIn::radius_minType map(1);
        REQUIRE_THROWS_AS(check_map(true, map, 0.0f), const char*);
    }
}

TEST_CASE("value") {
    io::GenerateIn::radius_minType map;
    map.push_back(std::vector<float>(2, 1.0f));
    map.back().back() = 2.0f;
    map.push_back(std::vector<float>(2, 3.0f));
    map.back().back() = 4.0f;
    check_map(true, map, 0.0f);
    SUBCASE("Corners") {
        REQUIRE(fabs(value(0.0f, 0.0f, map) - 1.0f) < 1e-7);
        REQUIRE(fabs(value(1.0f, 0.0f, map) - 2.0f) < 1e-7);
        REQUIRE(fabs(value(0.0f, 1.0f, map) - 3.0f) < 1e-7);
        REQUIRE(fabs(value(1.0f, 1.0f, map) - 4.0f) < 1e-7);
    }
    SUBCASE("Edge centers") {
        REQUIRE(fabs(value(0.5f, 0.0f, map) - 1.5f) < 1e-7);
        REQUIRE(fabs(value(1.0f, 0.5f, map) - 3.0f) < 1e-7);
        REQUIRE(fabs(value(0.5f, 1.0f, map) - 3.5f) < 1e-7);
        REQUIRE(fabs(value(0.0f, 0.5f, map) - 2.0f) < 1e-7);
    }
    SUBCASE("Center") {
        REQUIRE(fabs(value(0.5f, 0.5f, map) - 2.5f) < 1e-7);
    }
}

TEST_CASE("generate_change") {
    std::vector<float> change(4, 1.0f);
    SUBCASE("Touch correct") {
        generate_change(change,
            [](float x, float y, float r) { return 2.0f; },
            [](float x, float y, float r) { return 3.0f; });
        REQUIRE(change[0] == 1.0f);
        REQUIRE(change[1] == 1.0f);
        REQUIRE(change[2] == 2.0f);
        REQUIRE(change[3] == 3.0f);
    }
}

TEST_CASE("minmax") {
    io::GenerateIn::radius_minType map;
    map.push_back(std::vector<float>(2, 1.0f));
    map.back().back() = 2.0f;
    map.push_back(std::vector<float>(2, 3.0f));
    map.back().back() = 4.0f;
    check_map(true, map, 0.0f);
    SUBCASE("min == max") {
        REQUIRE(minmax(0.0f, 0.0f, 0.5f, map, map) == 1.0f);
        REQUIRE(minmax(0.0f, 0.0f, 0.0f, map, map) == 1.0f);
        REQUIRE(minmax(0.0f, 0.0f, 1.0f, map, map) == 1.0f);
    }
    io::GenerateIn::radius_minType map2;
    map2.push_back(std::vector<float>(2, 2.0f));
    map2.back().back() = 3.0f;
    map2.push_back(std::vector<float>(2, 4.0f));
    map2.back().back() = 5.0f;
    check_map(true, map2, 0.0f);
    SUBCASE("min + 1 == max") {
        REQUIRE(minmax(0.0f, 0.0f, 0.5f, map, map2) == 1.5f);
        REQUIRE(minmax(0.0f, 0.0f, 0.0f, map, map2) == 1.0f);
        REQUIRE(minmax(0.0f, 0.0f, 1.0f, map, map2) == 2.0f);
    }
}

TEST_CASE("minrange") {
    io::GenerateIn::radius_minType map;
    map.push_back(std::vector<float>(1, 1.0f));
    check_map(true, map, 0.0f);
    SUBCASE("min == range") {
        REQUIRE(minrange(0.0f, 0.0f, 0.5f, map, map) == 1.5f);
        REQUIRE(minrange(0.0f, 0.0f, 0.0f, map, map) == 1.0f);
        REQUIRE(minrange(0.0f, 0.0f, 1.0f, map, map) == 2.0f);
    }
}

TEST_CASE("maxrange") {
    io::GenerateIn::radius_minType map;
    map.push_back(std::vector<float>(1, 1.0f));
    check_map(true, map, 0.0f);
    SUBCASE("max == range") {
        REQUIRE(maxrange(0.0f, 0.0f, 0.5f, map, map) == 0.5f * map[0][0]);
        REQUIRE(maxrange(0.0f, 0.0f, 0.0f, map, map) == map[0][0]);
        REQUIRE(maxrange(0.0f, 0.0f, 1.0f, map, map) == 0.0f);
    }
}

#endif
