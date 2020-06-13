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
    // Check if bounding box of the circle intersects with target rectangle.
    if (X + R < Left)
        return false;
    if (Right < X - R)
        return false;
    if (Y + R < Low)
        return false;
    if (High < Y - R)
        return false;
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

static int render(io::RenderChangesIn& Val) {
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

TEST_CASE("delta_range") {
    REQUIRE(delta_range(-1.0, 0.0, 2.0) == 1.0);
    REQUIRE(delta_range(0.0, 0.0, 2.0) == 0.0);
    REQUIRE(delta_range(1.0, 0.0, 2.0) == 0.0);
    REQUIRE(delta_range(2.0, 0.0, 2.0) == 0.0);
    REQUIRE(delta_range(3.0, 0.0, 2.0) == 1.0);
}

TEST_CASE("check_overlap") {
    const double c = 1.0;
    const double xl = 0.0;
    const double xh = 2.0 * c;
    const double yl = 0.0;
    const double yh = 2.0 * c;
    io::RenderChangesIn::changesType scaled;
    SUBCASE("inside") {
        scaled.resize(0);
        REQUIRE(check_overlap(scaled, c, c, c, c, xl, xh, yl, yh));
        REQUIRE(scaled.size() == 1);
        REQUIRE(scaled.front().size() == 4);
        for (auto& v : scaled.front())
            REQUIRE(v == c);
    }
    SUBCASE("left out") {
        scaled.resize(0);
        REQUIRE(!check_overlap(scaled, xl - 2.0 * c, c, c, c, xl, xh, yl, yh));
        REQUIRE(scaled.empty());
    }
    SUBCASE("left partial") {
        scaled.resize(0);
        const double x = xl - 0.5 * c;
        REQUIRE(check_overlap(scaled, x, c, c, c, xl, xh, yl, yh));
        REQUIRE(scaled.size() == 1);
        REQUIRE(scaled.front().size() == 4);
        REQUIRE(scaled.front()[0] == x);
        REQUIRE(scaled.front()[1] == c);
        REQUIRE(scaled.front()[2] == c);
        REQUIRE(scaled.front()[3] == c);
    }
    SUBCASE("right partial") {
        scaled.resize(0);
        const double x = xh + 0.5 * c;
        REQUIRE(check_overlap(scaled, x, c, c, c, xl, xh, yl, yh));
        REQUIRE(scaled.size() == 1);
        REQUIRE(scaled.front().size() == 4);
        REQUIRE(scaled.front()[0] == x);
        REQUIRE(scaled.front()[1] == c);
        REQUIRE(scaled.front()[2] == c);
        REQUIRE(scaled.front()[3] == c);
    }
    SUBCASE("right out") {
        scaled.resize(0);
        REQUIRE(!check_overlap(scaled, xh + 2.0 * c, c, c, c, xl, xh, yl, yh));
        REQUIRE(scaled.empty());
    }
    SUBCASE("bottom out") {
        scaled.resize(0);
        REQUIRE(!check_overlap(scaled, c, yl - 2.0 * c, c, c, xl, xh, yl, yh));
        REQUIRE(scaled.empty());
    }
    SUBCASE("left partial") {
        scaled.resize(0);
        const double y = yl - 0.5 * c;
        REQUIRE(check_overlap(scaled, c, y, c, c, xl, xh, yl, yh));
        REQUIRE(scaled.size() == 1);
        REQUIRE(scaled.front().size() == 4);
        REQUIRE(scaled.front()[0] == c);
        REQUIRE(scaled.front()[1] == y);
        REQUIRE(scaled.front()[2] == c);
        REQUIRE(scaled.front()[3] == c);
    }
    SUBCASE("right partial") {
        scaled.resize(0);
        const double y = yh + 0.5 * c;
        REQUIRE(check_overlap(scaled, c, y, c, c, xl, xh, yl, yh));
        REQUIRE(scaled.size() == 1);
        REQUIRE(scaled.front().size() == 4);
        REQUIRE(scaled.front()[0] == c);
        REQUIRE(scaled.front()[1] == y);
        REQUIRE(scaled.front()[2] == c);
        REQUIRE(scaled.front()[3] == c);
    }
    SUBCASE("top out") {
        scaled.resize(0);
        REQUIRE(!check_overlap(scaled, c, yh + 2.0 * c, c, c, xl, xh, yl, yh));
        REQUIRE(scaled.empty());
    }
}

TEST_CASE("scale_changes") {
    const double c = 1.0;
    const double r = 0.5 * c;
    const double xl = 0.0;
    const double xh = 2.0 * c;
    const double yl = 0.0;
    const double yh = yl + xh;
    io::RenderChangesIn::changesType changes(1);
    io::RenderChangesIn::changesType scaled;
    SUBCASE("Inside") {
        changes.back() = std::vector<double> { 0.5, 0.5, r, c };
        scaled.resize(0);
        scale_changes(scaled, changes, xh, c, xl, xh, yl, yh);
        REQUIRE(scaled.size() == 1);
        REQUIRE(scaled.front().size() == 4);
        REQUIRE(scaled.front()[0] == c);
        REQUIRE(scaled.front()[1] == yl + c);
        REQUIRE(scaled.front()[2] == r);
        REQUIRE(scaled.front()[3] == c);
    }
    SUBCASE("Left edge") {
        changes.back() = std::vector<double> { 0.0, 0.5, r, c };
        scaled.resize(0);
        scale_changes(scaled, changes, xh, c, xl, xh, yl, yh);
        REQUIRE(scaled.size() == 2);
        for (auto& change : scaled) {
            REQUIRE(change.size() == 4);
            REQUIRE(change[1] == yl + c);
            REQUIRE(change[2] == r);
            REQUIRE(change[3] == c);
        }
        REQUIRE(fabs(scaled.front()[0] - scaled.back()[0]) == xh);
    }
    SUBCASE("Right edge") {
        changes.back() = std::vector<double> { 1.0, 0.5, r, c };
        scaled.resize(0);
        scale_changes(scaled, changes, xh, c, xl, xh, yl, yh);
        REQUIRE(scaled.size() == 2);
        for (auto& change : scaled) {
            REQUIRE(change.size() == 4);
            REQUIRE(change[1] == yl + c);
            REQUIRE(change[2] == r);
            REQUIRE(change[3] == c);
        }
        REQUIRE(fabs(scaled.front()[0] - scaled.back()[0]) == xh);
    }
    SUBCASE("Bottom edge") {
        changes.back() = std::vector<double> { 0.5, 0.0, r, c };
        scaled.resize(0);
        scale_changes(scaled, changes, xh, c, xl, xh, yl, yh);
        REQUIRE(scaled.size() == 2);
        for (auto& change : scaled) {
            REQUIRE(change.size() == 4);
            REQUIRE(change[0] == c);
            REQUIRE(change[2] == r);
            REQUIRE(change[3] == c);
        }
        REQUIRE(fabs(scaled.front()[1] - scaled.back()[1]) == xh);
    }
    SUBCASE("Top edge") {
        changes.back() = std::vector<double> { 0.5, 1.0, r, c };
        scaled.resize(0);
        scale_changes(scaled, changes, xh, c, xl, xh, yl, yh);
        REQUIRE(scaled.size() == 2);
        for (auto& change : scaled) {
            REQUIRE(change.size() == 4);
            REQUIRE(change[0] == c);
            REQUIRE(change[2] == r);
            REQUIRE(change[3] == c);
        }
        REQUIRE(fabs(scaled.front()[1] - scaled.back()[1]) == xh);
    }
    SUBCASE("Bottom left corner") {
        changes.back() = std::vector<double> { 0.0, 0.0, r, c };
        scaled.resize(0);
        scale_changes(scaled, changes, xh, c, xl, xh, yl, yh);
        REQUIRE(scaled.size() == 4);
        for (auto& change : scaled) {
            REQUIRE(change.size() == 4);
            REQUIRE(change[2] == r);
            REQUIRE(change[3] == c);
        }
    }
    SUBCASE("Top left corner") {
        changes.back() = std::vector<double> { 0.0, 1.0, r, c };
        scaled.resize(0);
        scale_changes(scaled, changes, xh, c, xl, xh, yl, yh);
        REQUIRE(scaled.size() == 4);
        for (auto& change : scaled) {
            REQUIRE(change.size() == 4);
            REQUIRE(change[2] == r);
            REQUIRE(change[3] == c);
        }
    }
    SUBCASE("Top right corner") {
        changes.back() = std::vector<double> { 1.0, 1.0, r, c };
        scaled.resize(0);
        scale_changes(scaled, changes, xh, c, xl, xh, yl, yh);
        REQUIRE(scaled.size() == 4);
        for (auto& change : scaled) {
            REQUIRE(change.size() == 4);
            REQUIRE(change[2] == r);
            REQUIRE(change[3] == c);
        }
    }
    SUBCASE("Bottom right corner") {
        changes.back() = std::vector<double> { 1.0, 0.0, r, c };
        scaled.resize(0);
        scale_changes(scaled, changes, xh, c, xl, xh, yl, yh);
        REQUIRE(scaled.size() == 4);
        for (auto& change : scaled) {
            REQUIRE(change.size() == 4);
            REQUIRE(change[2] == r);
            REQUIRE(change[3] == c);
        }
    }
}

TEST_CASE("pick_changes") {
    const double c = 1.0;
    const double r = 0.125 * c;
    const double xl = 0.0;
    const double xh = 2.0 * c;
    const double yl = 0.0;
    const double yh = yl + xh;
    io::RenderChangesIn::changesType changes;
    changes.push_back(std::vector<double> { 0.5, 0.0, r, c });
    changes.push_back(std::vector<double> { 0.5, 0.5, r, c });
    changes.push_back(std::vector<double> { 0.5, 1.0, r, c });
    io::RenderChangesIn::changesType scaled;
    scale_changes(scaled, changes, xh, c, xl, xh, yl, yh);
    REQUIRE(scaled.size() == 5);
    std::vector<std::vector<double>> spans;
    SUBCASE("First, center") {
        spans.resize(0);
        pick_changes(spans, scaled, scaled[0][1], xh);
        REQUIRE(spans.size() == 2);
        REQUIRE(spans.front().size() == 3);
        REQUIRE(spans.front()[0] == scaled[0][0]);
        REQUIRE(spans.front()[1] == r * r);
        REQUIRE(spans.front()[2] == scaled[0][3]);
    }
    SUBCASE("None") {
        spans.resize(0);
        pick_changes(spans, scaled, 0.5 * (scaled[0][1] + scaled[2][1]), xh);
        REQUIRE(spans.size() == 0);
    }
    SUBCASE("Second, edge") {
        spans.resize(0);
        pick_changes(spans, scaled, scaled[2][1] - r, xh);
        REQUIRE(spans.size() == 1);
        REQUIRE(spans.front().size() == 3);
        REQUIRE(spans.front()[0] == scaled[2][0]);
        REQUIRE(spans.front()[1] == 0.0);
        REQUIRE(spans.front()[2] == scaled[2][3]);
    }
    SUBCASE("Third") {
        spans.resize(0);
        pick_changes(spans, scaled, scaled[4][1] - 0.5 * r, xh);
        REQUIRE(spans.size() == 2);
        REQUIRE(spans.front().size() == 3);
        REQUIRE(spans.front()[0] == scaled[4][0]);
        REQUIRE(spans.front()[1] > 0.0);
        REQUIRE(spans.front()[1] < r * r);
        REQUIRE(spans.front()[2] == scaled[4][3]);
    }
}

TEST_CASE("compute_heights") {
    std::vector<std::vector<double>> spans;
    std::vector<float> row;
    SUBCASE("Center") {
        spans.resize(0);
        row.resize(5, 0.0f);
        spans.push_back(std::vector<double> { 2.0, 1.1, 1.0 });
        compute_heights(row, 0, row.size(), spans, row.size());
        REQUIRE(row[0] == 0.0f);
        REQUIRE(row[1] == 1.0f);
        REQUIRE(row[2] == 1.0f);
        REQUIRE(row[3] == 1.0f);
        REQUIRE(row[4] == 0.0f);
    }
    SUBCASE("Start") {
        spans.resize(0);
        row.resize(5, 0.0f);
        spans.push_back(std::vector<double> { 0.5, 1.1, 1.0 });
        compute_heights(row, 0, row.size(), spans, row.size());
        REQUIRE(row[0] == 1.0f);
        REQUIRE(row[1] == 1.0f);
        REQUIRE(row[2] == 0.0f);
        REQUIRE(row[3] == 0.0f);
        REQUIRE(row[4] == 0.0f);
    }
    SUBCASE("End") {
        spans.resize(0);
        row.resize(5, 0.0f);
        spans.push_back(std::vector<double> { 3.0, 1.1, 1.0 });
        compute_heights(row, 0, row.size(), spans, row.size());
        REQUIRE(row[0] == 0.0f);
        REQUIRE(row[1] == 0.0f);
        REQUIRE(row[2] == 1.0f);
        REQUIRE(row[3] == 1.0f);
        REQUIRE(row[4] == 1.0f);
    }
}

#endif
