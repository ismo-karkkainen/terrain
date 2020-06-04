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
    const std::uint32_t Size, const double MaxRadius)
{
    for (auto& change : Changes) {
        change[0] *= Size;
        change[1] *= Size;
        change[2] *= MaxRadius;
    }
}

static void pick_changes(std::vector<std::vector<double>>& Spans,
    const io::RenderChangesIn::changesType& Changes,
    const double Y, const double Size)
{
    Spans.resize(0);
    for (auto& change : Changes) {
        const double dy = std::min(abs(Y - change[1]),
            std::min(abs(Y + Size - change[1]),
                abs(Y - (change[1] + Size))));
        if (change[2] < dy)
            continue;
        Spans.push_back(change);
        Spans.back()[1] = Spans.back()[2] * Spans.back()[2] - dy * dy;
        // Could continue and crete one or two (wrap) spans and clip
        // against the [left, right[ range. Simplifies test further.
        // Uses more memory.
    }
}

static void compute_heights(std::vector<float>& Row,
    const std::uint32_t Left, const std::uint32_t Right,
    const std::vector<std::vector<double>>& Spans, const double Size)
{
    for (std::uint32_t x = Left; x < Right; ++x) {
        double delta = 0.0;
        for (auto& change : Spans) {
            const double dx = std::min(abs(x - change[0]),
                std::min(abs(x + Size - change[0]),
                    abs(x - (change[0] + Size))));
            if (change[1] < dx * dx)
                continue;
            delta += change[3];
        }
        Row[x - Left] = float(delta);
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
    scale_changes(Val.changes(), size, 0.5 * Val.size());
    std::vector<char> buffer;
    std::vector<float> row;
    if (left < right)
        row.resize(right - left);
    std::vector<std::vector<double>> spans;
    for (std::uint32_t y = low; y < high; ++y) {
        pick_changes(spans, Val.changes(), y, size);
        compute_heights(row, left, right, spans, size);
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
        changes.back() = std::vector<double> { 0.5, 0.5, 0.25, 1.0 };
        scale_changes(changes, 4, 2.0f);
        REQUIRE(changes.size() == 8);
        REQUIRE(changes.front()[0] == 2.0f);
        REQUIRE(changes.front()[1] == 2.0f);
        REQUIRE(changes.front()[2] == 0.25f);
        REQUIRE(changes.front()[3] == 1.0f);
        REQUIRE(changes.front()[4] == 1.5f);
        REQUIRE(changes.front()[5] == 2.5f);
        REQUIRE(changes.front()[6] == 1.5f);
        REQUIRE(changes.front()[7] == 2.5f);
    }
}

#endif
