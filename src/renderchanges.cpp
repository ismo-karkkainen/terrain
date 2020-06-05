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


static double delta_range(const double V, const double Low, const double High) {
    if (V < Low)
        return Low - V;
    if (High < V)
        return V - High;
    return 0.0;
}

// Return value indicates if bounding box overlapped, i.e. close enough for
// the placement in +/- Size to be well outside.
static bool check_overlap(io::RenderChangesIn::changesType& Scaled,
    const double X, const double Y, const double R, const double D,
    const double Left, const double Right, const double Low, const double High)
{
    if (X + R < Left)
        return false;
    if (Right < X - R)
        return false;
    if (Y + R < Low)
        return false;
    if (High < Y - R)
        return false;
    // Target rectangle and circle's bounding box intersect.
    // Check if circle intersects with target rectangle.
    const double dx = delta_range(X, Left, Right);
    const double dy = delta_range(Y, Low, High);
    if (R * R < dx * dx + dy * dy)
        return true;
    Scaled.push_back(std::vector<double> { X, Y, R, D });
    return true;
}

static void scale_changes(io::RenderChangesIn::changesType& Scaled,
    const io::RenderChangesIn::changesType& Changes, const double Size,
    const double MaxRadius, const double Left, const double Right,
    const double Low, const double High)
{
    Scaled.resize(0);
    for (auto& change : Changes) {
        const double x = change[0] * Size;
        const double y = change[1] * Size;
        const double r = change[2] * MaxRadius;
        const double d = change[3];
        check_overlap(Scaled, x, y, r, d, Left, Right, Low, High);
        if (!check_overlap(Scaled, x - Size, y, r, d, Left, Right, Low, High))
            check_overlap(Scaled, x + Size, y, r, d, Left, Right, Low, High);
        if (!check_overlap(Scaled, x, y - Size, r, d, Left, Right, Low, High))
            check_overlap(Scaled, x, y + Size, r, d, Left, Right, Low, High);
        if (!check_overlap(Scaled, x - Size, y - Size, r, d, Left, Right, Low, High))
            check_overlap(Scaled, x + Size, y + Size, r, d, Left, Right, Low, High);
        if (!check_overlap(Scaled, x - Size, y + Size, r, d, Left, Right, Low, High))
            check_overlap(Scaled, x + Size, y - Size, r, d, Left, Right, Low, High);
    }
}

static void pick_changes(std::vector<std::vector<double>>& Spans,
    const io::RenderChangesIn::changesType& Changes,
    const double Y, const double Size)
{
    Spans.resize(0);
    for (auto& change : Changes) {
        if (change[1] + change[2] < Y)
            continue;
        if (Y < change[1] - change[2])
            continue;
        const double dy = Y - change[1];
        Spans.push_back(std::vector<double> {
            change[0], change[2] * change[2] - dy * dy, change[3] });
    }
}

// Use 64-bit integers for height computations, find scale using maximum
// change and prior to writing the results, map back. New parameter.
// Speed-up using doubles first to see how fast that can be.

static void compute_heights(std::vector<float>& Row,
    const std::uint32_t Left, const std::uint32_t Right,
    const std::vector<std::vector<double>>& Spans, const double Size)
{
    for (std::uint32_t x = Left; x < Right; ++x) {
        double delta = 0.0;
        for (auto& change : Spans) {
            const double dx = x - change[0];
            if (dx * dx <= change[1])
                delta += change[2];
        }
        Row[x - Left] = delta;
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
    io::RenderChangesIn::changesType scaled;
    scale_changes(scaled, Val.changes(), size, 0.5 * Val.size(), left, right, low, high);
    std::vector<char> buffer;
    std::vector<float> row;
    if (left < right)
        row.resize(right - left);
    std::vector<std::vector<double>> spans;
    for (std::uint32_t y = low; y < high; ++y) {
        pick_changes(spans, scaled, y, size);
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
    io::RenderChangesIn::changesType scaled;
    SUBCASE("Halves") {
        changes.back() = std::vector<double> { 0.5, 0.5, 0.25, 1.0 };
        scale_changes(scaled, changes, 4, 2.0f, 0, 4, 0, 4);
        REQUIRE(scaled.size() == 1);
        REQUIRE(scaled.front().size() == 1);
        REQUIRE(scaled.front()[0] == 2.0f);
        REQUIRE(scaled.front()[1] == 2.0f);
        REQUIRE(scaled.front()[2] == 0.25f);
        REQUIRE(scaled.front()[3] == 1.0f);
    }
}

#endif
