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
#include "FileDescriptorInput.hpp"
#include "BlockQueue.hpp"
#endif
#include "render_io.hpp"
#include <vector>
#include <iostream>
#include <cmath>
#include <ctime>
#include <cinttypes>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>


static void scale_changes(io::RenderChangesIn::changesType& Changes,
    const std::uint32_t Size, const float MaxRadius)
{
    for (auto& change : Changes) {
        change[0] *= Size;
        change[1] *= Size;
        change[2] *= MaxRadius;
    }
}

static void row_deltas(const io::RenderChangesIn::changesType& Changes,
    std::vector<float>& Deltas, std::uint32_t Y, const std::uint32_t Size,
    const std::uint32_t Left, const std::uint32_t Right)
{
    for (auto& change : Changes) {
        const float radius = change[2];
        const float diff = fabs(change[1] - Y);
        if (radius < diff)
            continue;
        const float line_span = sqrtf(radius * radius - diff * diff);
        float from = round(change[0] - line_span);
        float to = round(change[0] + line_span);
        if (0 < from && Right < from) // On the right side of range.
            continue;
        if (to < Size && to < Left) // On the left side of range.
            continue;
        // Wraps around?
        if (from < 0)
            from += Size;
        else if (Size <= to)
            to -= Size;
        if (to < from) {
            if (to < Left && Right < from)
                continue;
            Deltas.front() += change[3];
            Deltas[to + 1] -= change[3];
            Deltas[from] += change[3];
        } else if (to == from) {
            Deltas.front() += change[3];
        } else {
            Deltas[from] += change[3];
            Deltas[to] -= change[3];
        }
    }
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
    const float max_radius = 0.5f * Val.size();
    scale_changes(Val.changes(), size, max_radius);
    std::vector<char> buffer;
    std::vector<float> deltas(size + 1, 0.0f);
    std::vector<float> row;
    if (left < right)
        row.resize(right - left);
    for (std::uint32_t y = low; y < high; ++y) {
        if (left < right) {
            row_deltas(Val.changes(), deltas, y, size, left, right);
            float height = 0.0f;
            for (std::uint32_t n = 0; n < left; ++n) {
                height += deltas[n];
                deltas[n] = 0.0f;
            }
            for (std::uint32_t n = left; n < right; ++n) {
                height += deltas[n];
                deltas[n] = 0.0f;
                row[n - left] = height;
            }
            for (std::uint32_t n = right; n < size; ++n)
                deltas[n] = 0.0f;
        }
        io::Write(std::cout, row, buffer);
        if (y + 1 != high)
            std::cout << ',';
    }
}

static const size_t block_size = 1048576;

int main(int argc, char** argv) {
    int f = 0;
    if (argc > 1)
        f = open(argv[1], O_RDONLY);
    FileDescriptorInput input(f);
    BlockQueue::BlockPtr buffer;
    io::ParserPool pp;
    io::RenderChangesIn_Parser parser;
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
        io::RenderChangesIn val;
        parser.Swap(val.values);
        std::cout << "{\"heightfield\":[";
        render_changes(val);
        std::cout << "]}" << std::endl;
    }
    if (f)
        close(f);
    return 0;
}

#else

TEST_CASE("scale_changes") {
    io::RenderChangesIn::changesType changes(1);
    SUBCASE("Halves") {
        changes.back() = std::vector<float> { 0.5f, 0.5f, 0.5f, 1.0f };
        scale_changes(changes, 4, 2.0f);
        REQUIRE(changes.front()[0] == 2.0f);
        REQUIRE(changes.front()[1] == 2.0f);
        REQUIRE(changes.front()[2] == 1.0f);
        REQUIRE(changes.front()[3] == 1.0f);
    }
}

TEST_CASE("row_deltas") {
    io::RenderChangesIn::changesType changes(1);
    SUBCASE("Within") {
        changes.back() = std::vector<float> { 2.0f, 2.0f, 0.98f, 1.0f };
        std::vector<float> deltas(5, 0.0f);
        row_deltas(changes, deltas, 2, 4, 0, 4);
        REQUIRE(deltas[0] == 0.0f);
        REQUIRE(deltas[1] == 1.0f);
        REQUIRE(deltas[2] == 0.0f);
        REQUIRE(deltas[3] == -1.0f);
        REQUIRE(deltas[4] == 0.0f);
    }
    SUBCASE("Wrap") {
        changes.back() = std::vector<float> { 3.99f, 2.0f, 1.0f, 1.0f };
        std::vector<float> deltas(5, 0.0f);
        row_deltas(changes, deltas, 2, 4, 0, 4);
        REQUIRE(deltas[0] == 1.0f);
        REQUIRE(deltas[1] == 0.0f);
        REQUIRE(deltas[2] == -1.0f);
        REQUIRE(deltas[3] == 1.0f);
        REQUIRE(deltas[4] == 0.0f);
    }
    SUBCASE("Entire row") {
        changes.back() = std::vector<float> { 2.0f, 2.0f, 2.0f, 1.0f };
        std::vector<float> deltas(5, 0.0f);
        row_deltas(changes, deltas, 2, 4, 0, 4);
        REQUIRE(deltas[0] == 1.0f);
        REQUIRE(deltas[1] == 0.0f);
        REQUIRE(deltas[2] == 0.0f);
        REQUIRE(deltas[3] == 0.0f);
        REQUIRE(deltas[4] == 0.0f);
    }
    SUBCASE("Entire row wrapped") {
        changes.back() = std::vector<float> { 3.0f, 2.0f, 2.0f, 1.0f };
        std::vector<float> deltas(5, 0.0f);
        row_deltas(changes, deltas, 2, 4, 0, 4);
        REQUIRE(deltas[0] == 1.0f);
        REQUIRE(deltas[1] == 0.0f);
        REQUIRE(deltas[2] == 0.0f);
        REQUIRE(deltas[3] == 0.0f);
        REQUIRE(deltas[4] == 0.0f);
    }
    SUBCASE("Left side of range") {
        changes.back() = std::vector<float> { 1.0f, 2.0f, 1.0f, 1.0f };
        std::vector<float> deltas(5, 0.0f);
        row_deltas(changes, deltas, 2, 4, 3, 4);
        REQUIRE(deltas[0] == 0.0f);
        REQUIRE(deltas[1] == 0.0f);
        REQUIRE(deltas[2] == 0.0f);
        REQUIRE(deltas[3] == 0.0f);
        REQUIRE(deltas[4] == 0.0f);
    }
    SUBCASE("Right side of range") {
        changes.back() = std::vector<float> { 3.0f, 2.0f, 1.0f, 1.0f };
        std::vector<float> deltas(5, 0.0f);
        row_deltas(changes, deltas, 2, 4, 0, 1);
        REQUIRE(deltas[0] == 0.0f);
        REQUIRE(deltas[1] == 0.0f);
        REQUIRE(deltas[2] == 0.0f);
        REQUIRE(deltas[3] == 0.0f);
        REQUIRE(deltas[4] == 0.0f);
    }
    SUBCASE("Outside range wrapped via left") {
        changes.back() = std::vector<float> { 0.0f, 2.0f, 1.0f, 1.0f };
        std::vector<float> deltas(5, 0.0f);
        row_deltas(changes, deltas, 2, 4, 2, 3);
        REQUIRE(deltas[0] == 0.0f);
        REQUIRE(deltas[1] == 0.0f);
        REQUIRE(deltas[2] == 0.0f);
        REQUIRE(deltas[3] == 0.0f);
        REQUIRE(deltas[4] == 0.0f);
    }
    SUBCASE("Outside range wrapped via right") {
        changes.back() = std::vector<float> { 4.0f, 2.0f, 1.0f, 1.0f };
        std::vector<float> deltas(6, 0.0f);
        row_deltas(changes, deltas, 2, 5, 1, 2);
        REQUIRE(deltas[0] == 0.0f);
        REQUIRE(deltas[1] == 0.0f);
        REQUIRE(deltas[2] == 0.0f);
        REQUIRE(deltas[3] == 0.0f);
        REQUIRE(deltas[4] == 0.0f);
        REQUIRE(deltas[5] == 0.0f);
    }
}

#endif
