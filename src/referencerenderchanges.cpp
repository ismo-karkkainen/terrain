//
//  renderchanges.cpp
//
//  Created by Ismo Kärkkäinen on 27.5.2020.
//  Copyright © 2020 Ismo Kärkkäinen. All rights reserved.
//
// Licensed under Universal Permissive License. See License.txt.

#if defined(UNITTEST)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#else
#include "convenience.hpp"
#endif
#include "render_io.hpp"
#include <vector>
#include <iostream>
#include <cmath>
#include <cinttypes>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>


static void scale_changes(io::RenderChangesIn::changesType& Changes,
    const std::uint32_t Size, const double MaxRadius)
{
    for (auto& change : Changes) {
        change[0] *= Size;
        change[1] *= Size;
        change[2] *= MaxRadius;
        change[2] *= change[2];
    }
}

static bool absasc(const std::vector<double>& a, const std::vector<double>& b) {
    return abs(a[3]) < abs(b[3]);
}

#if !defined(UNITTEST)
static void render_changes(io::RenderChangesIn& Val) {
    const std::uint32_t size = Val.size();
    const std::uint32_t low = Val.lowGiven() ? std::min(Val.low(), size) : 0;
    const std::uint32_t high = Val.highGiven() ? std::min(Val.high(), size) : size;
    const std::uint32_t left = Val.leftGiven() ? std::min(Val.left(), size) : 0;
    const std::uint32_t right = Val.rightGiven() ? std::min(Val.right(), size) : size;
    if (high <= low)
        return;
    scale_changes(Val.changes(), size, 0.5 * Val.size());
    std::vector<char> buffer;
    std::vector<float> row;
    if (left < right)
        row.resize(right - left);
    for (std::uint32_t y = low; y < high; ++y) {
        for (std::uint32_t x = left; x < right; ++x) {
            double sn = 0.0;
            double sp = 0.0;
            for (auto& change : Val.changes()) {
                const double dx = std::min(abs(x - change[0]),
                    std::min(abs(x + size - change[0]),
                        abs(x - (change[0] + size))));
                if (change[2] < dx * dx)
                    continue;
                const double dy = std::min(abs(y - change[1]),
                    std::min(abs(y + size - change[1]),
                        abs(y - (change[1] + size))));
                if (change[2] < dx * dx + dy * dy)
                    continue;
                if (change[3] < 0)
                    sn += change[3];
                else
                    sp += change[3];
            }
            row[x - left] = float(sp + sn);
        }
        io::Write(std::cout, row, buffer);
        if (y + 1 != high)
            std::cout << ',';
    }
}

static int render(io::RenderChangesIn& Val) {
    std::sort(Val.changes().begin(), Val.changes().end(), absasc);
    std::cout << "{\"heightfield\":[";
    render_changes(Val);
    std::cout << "]}" << std::endl;
    return 0;
}

int main(int argc, char** argv) {
    int f = 0;
    if (argc > 1)
        f = open(argv[1], O_RDONLY);
    InputParser<io::ParserPool, io::RenderChangesIn_Parser,
        io::RenderChangesIn> ip(f);
    int status = ip.ReadAndParse(render);
    if (f)
        close(f);
    return status;
}

#else

TEST_CASE("scale_changes") {
    io::RenderChangesIn::changesType changes(1);
    SUBCASE("Halves") {
        changes.back() = std::vector<double> { 0.5, 0.5, 0.5, 1.0 };
        scale_changes(changes, 4, 2.0f);
        REQUIRE(changes.front()[0] == 2.0f);
        REQUIRE(changes.front()[1] == 2.0f);
        REQUIRE(changes.front()[2] == 1.0f);
        REQUIRE(changes.front()[3] == 1.0f);
    }
}

TEST_CASE("absasc") {
    SUBCASE("Reverse") {
        io::RenderChangesIn::changesType changes;
        changes.push_back(std::vector<double> { 0.0, 0.0, 0.0, 1.0 });
        changes.push_back(std::vector<double> { 0.0, 0.0, 0.0, -0.5 });
        std::sort(changes.begin(), changes.end(), absasc);
        REQUIRE(changes[0][3] == -0.5);
        REQUIRE(changes[1][3] == 1.0);
    }
    SUBCASE("Retain") {
        io::RenderChangesIn::changesType changes;
        changes.push_back(std::vector<double> { 0.0, 0.0, 0.0, 1.0 });
        changes.push_back(std::vector<double> { 0.0, 0.0, 0.0, -3.0 });
        std::sort(changes.begin(), changes.end(), absasc);
        REQUIRE(changes[0][3] == 1.0);
        REQUIRE(changes[1][3] == -3.0);
    }
}

#endif
