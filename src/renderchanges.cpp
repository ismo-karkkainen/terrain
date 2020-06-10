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


struct ScaledChange {
    double x, y, r;
    std::int64_t c;
    ScaledChange() : x(0.0), y(0.0), r(0.0), c(0) { }
    ScaledChange(double X, double Y, double R, std::int64_t C) :
        x(X), y(Y), r(R), c(C) { }
};

static double delta_range(const double V, const double Low, const double High) {
    if (V < Low)
        return Low - V;
    if (High < V)
        return V - High;
    return 0.0;
}

static double max_abs_change(const io::RenderChangesIn::changesType& Changes) {
    double maxabs = 0.0;
    for (auto& change : Changes) {
        double cand = abs(change[3]);
        if (maxabs < cand)
            maxabs = cand;
    }
    return maxabs;
}

// Return value indicates if bounding box overlapped, i.e. close enough for
// the placement in +/- Size to be well outside.
static bool check_overlap(std::vector<ScaledChange>& Scaled,
    const double X, const double Y, const double R, const std::int64_t D,
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
    Scaled.push_back(ScaledChange(X, Y, R, D));
    return true;
}

static void scale_changes(std::vector<ScaledChange>& Scaled,
    const io::RenderChangesIn::changesType& Changes, const double Size,
    const double MaxRadius, const double ChangeScale,
    const double Left, const double Right, const double Low, const double High)
{
    Scaled.resize(0);
    for (auto& change : Changes) {
        const double x = change[0] * Size;
        const double y = change[1] * Size;
        const double r = change[2] * MaxRadius;
        const std::int64_t d =
            static_cast<std::int64_t>(round(change[3] * ChangeScale));
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

static void row_deltas(std::vector<std::int64_t>& Deltas,
    const std::vector<ScaledChange>& Scaled, const double Y,
    const double Size, const double Left, const double Right)
{
    for (auto& change : Scaled) {
        double diff = abs(Y - change.y);
        if (change.r < diff)
            continue;
        double line_span = sqrt(change.r * change.r - diff * diff);
        double from = change.x - line_span;
        if (from <= 0.0)
            from = 0.0;
        else if (Right < from)
            continue;
        else {
            // Check few adjacent values and pick smallest x that is within.
            bool found = false;
            for (double cx = std::max(0.0, floor(from) - 1.0);
                cx < std::min(ceil(from) + 1.0, Size); ++cx)
            {
                double dx = cx - change.x;
                if (dx * dx + diff * diff <= change.r * change.r) {
                    from = cx;
                    found = true;
                    break;
                }
            }
            if (!found || Size <= from)
                continue; // Border-line case, no integer coordinate is inside.
        }
        double to = change.x + line_span;
        if (Size <= to)
            to = Size;
        else if (to < Left)
            continue;
        else {
            // Check few adjacent values and pick smallest x that is outside.
            for (double cx = std::max(from + 1.0, floor(to) - 1.0);
                cx <= std::min(ceil(to) + 1.0, Size); ++cx)
            {
                double dx = cx - change.x;
                if (change.r * change.r < dx * dx + diff * diff) {
                    to = cx;
                    break;
                }
            }
        }
        Deltas[from] += change.c;
        Deltas[to] -= change.c;
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
    const double max = max_abs_change(Val.changes());

    std::int64_t change_room;
    if ((1 << 12) < Val.changes().size())
        change_room = 1LL << (63 - int(ceil(log2(Val.changes().size()))));
    else
        change_room = 1LL << 51;
    const double change_scale = floor(static_cast<double>(change_room) / max);
    std::vector<ScaledChange> scaled;
    scale_changes(scaled, Val.changes(), size, 0.5 * Val.size(), change_scale,
        left, right, low, high);
    std::vector<char> buffer;
    std::vector<std::int64_t> deltas(size + 1, 0);
    std::vector<float> row;
    if (left < right)
        row.resize(right - left);
    for (std::uint32_t y = low; y < high; ++y) {
        if (left < right) {
            row_deltas(deltas, scaled, y, size, left, right);
            std::int64_t height = 0;
            for (std::uint32_t n = 0; n < left; ++n) {
                height += deltas[n];
                deltas[n] = 0;
            }
            for (std::uint32_t n = left; n < right; ++n) {
                height += deltas[n];
                deltas[n] = 0;
                row[n - left] = height / change_scale;
            }
            for (std::uint32_t n = right; n < deltas.size(); ++n)
                deltas[n] = 0;
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
    const std::int64_t cc = 1024;
    std::vector<ScaledChange> scaled;
    SUBCASE("inside") {
        scaled.resize(0);
        REQUIRE(check_overlap(scaled, c, c, c, cc, xl, xh, yl, yh));
        REQUIRE(scaled.size() == 1);
        REQUIRE(scaled.front().x == c);
        REQUIRE(scaled.front().y == c);
        REQUIRE(scaled.front().r == c);
        REQUIRE(scaled.front().c == cc);
    }
    SUBCASE("left out") {
        scaled.resize(0);
        REQUIRE(!check_overlap(scaled, xl - 2.0 * c, c, c, cc, xl, xh, yl, yh));
        REQUIRE(scaled.empty());
    }
    SUBCASE("left partial") {
        scaled.resize(0);
        const double x = xl - 0.5 * c;
        REQUIRE(check_overlap(scaled, x, c, c, cc, xl, xh, yl, yh));
        REQUIRE(scaled.size() == 1);
        REQUIRE(scaled.front().x == x);
        REQUIRE(scaled.front().y == c);
        REQUIRE(scaled.front().r == c);
        REQUIRE(scaled.front().c == cc);
    }
    SUBCASE("right partial") {
        scaled.resize(0);
        const double x = xh + 0.5 * c;
        REQUIRE(check_overlap(scaled, x, c, c, cc, xl, xh, yl, yh));
        REQUIRE(scaled.size() == 1);
        REQUIRE(scaled.front().x == x);
        REQUIRE(scaled.front().y == c);
        REQUIRE(scaled.front().r == c);
        REQUIRE(scaled.front().c == cc);
    }
    SUBCASE("right out") {
        scaled.resize(0);
        REQUIRE(!check_overlap(scaled, xh + 2.0 * c, c, c, cc, xl, xh, yl, yh));
        REQUIRE(scaled.empty());
    }
    SUBCASE("bottom out") {
        scaled.resize(0);
        REQUIRE(!check_overlap(scaled, c, yl - 2.0 * c, c, cc, xl, xh, yl, yh));
        REQUIRE(scaled.empty());
    }
    SUBCASE("left partial") {
        scaled.resize(0);
        const double y = yl - 0.5 * c;
        REQUIRE(check_overlap(scaled, c, y, c, cc, xl, xh, yl, yh));
        REQUIRE(scaled.size() == 1);
        REQUIRE(scaled.front().x == c);
        REQUIRE(scaled.front().y == y);
        REQUIRE(scaled.front().r == c);
        REQUIRE(scaled.front().c == cc);
    }
    SUBCASE("right partial") {
        scaled.resize(0);
        const double y = yh + 0.5 * c;
        REQUIRE(check_overlap(scaled, c, y, c, cc, xl, xh, yl, yh));
        REQUIRE(scaled.size() == 1);
        REQUIRE(scaled.front().x == c);
        REQUIRE(scaled.front().y == y);
        REQUIRE(scaled.front().r == c);
        REQUIRE(scaled.front().c == cc);
    }
    SUBCASE("top out") {
        scaled.resize(0);
        REQUIRE(!check_overlap(scaled, c, yh + 2.0 * c, c, cc, xl, xh, yl, yh));
        REQUIRE(scaled.empty());
    }
}

TEST_CASE("max_abs_change") {
    io::RenderChangesIn::changesType changes;
    SUBCASE("Only one") {
        changes.resize(0);
        changes.push_back(std::vector<double> { 0.0, 0.0, 0.0, 1.0 });
        REQUIRE(max_abs_change(changes) == 1.0);
    }
    SUBCASE("Max second") {
        changes.resize(0);
        changes.push_back(std::vector<double> { 0.0, 0.0, 0.0, 1.0 });
        changes.push_back(std::vector<double> { 0.0, 0.0, 0.0, 2.0 });
        REQUIRE(max_abs_change(changes) == 2.0);
    }
    SUBCASE("Max first") {
        changes.resize(0);
        changes.push_back(std::vector<double> { 0.0, 0.0, 0.0, 2.0 });
        changes.push_back(std::vector<double> { 0.0, 0.0, 0.0, 1.0 });
        REQUIRE(max_abs_change(changes) == 2.0);
    }
    SUBCASE("Max first negative") {
        changes.resize(0);
        changes.push_back(std::vector<double> { 0.0, 0.0, 0.0, -2.0 });
        changes.push_back(std::vector<double> { 0.0, 0.0, 0.0, 1.0 });
        REQUIRE(max_abs_change(changes) == 2.0);
    }
    SUBCASE("Max second with negative") {
        changes.resize(0);
        changes.push_back(std::vector<double> { 0.0, 0.0, 0.0, -1.0 });
        changes.push_back(std::vector<double> { 0.0, 0.0, 0.0, 2.0 });
        REQUIRE(max_abs_change(changes) == 2.0);
    }
}

static bool scaled_less(const ScaledChange& A, const ScaledChange& B) {
    if (A.x != B.x)
        return A.x < B.x;
    return A.y < B.y;
}

TEST_CASE("scale_changes") {
    io::RenderChangesIn::changesType changes(1);
    std::vector<ScaledChange> scaled;
    SUBCASE("Within") {
        scaled.resize(0);
        changes.back() = std::vector<double> { 0.5, 0.5, 0.5, 1.0 };
        scale_changes(scaled, changes, 4.0, 2.0, 1.0, 0.0, 4.0, 0.0, 4.0);
        REQUIRE(scaled.size() == 1);
        REQUIRE(scaled.front().x == 2.0);
        REQUIRE(scaled.front().y == 2.0);
        REQUIRE(scaled.front().r == 1.0);
        REQUIRE(scaled.front().c == 1);
    }
    SUBCASE("Wrap left") {
        scaled.resize(0);
        changes.back() = std::vector<double> { 0.0, 0.5, 0.5, 1.0 };
        scale_changes(scaled, changes, 4.0, 2.0, 1.0, 0.0, 4.0, 0.0, 4.0);
        REQUIRE(scaled.size() == 2);
        REQUIRE(scaled[0].x == 0.0);
        REQUIRE(scaled[1].x == 4.0);
    }
    SUBCASE("Wrap right") {
        scaled.resize(0);
        changes.back() = std::vector<double> { 1.0, 0.5, 0.5, 1.0 };
        scale_changes(scaled, changes, 4.0, 2.0, 1.0, 0.0, 4.0, 0.0, 4.0);
        REQUIRE(scaled.size() == 2);
        REQUIRE(scaled[0].x == 4.0);
        REQUIRE(scaled[1].x == 0.0);
    }
    SUBCASE("Wrap bottom") {
        scaled.resize(0);
        changes.back() = std::vector<double> { 0.5, 0.0, 0.5, 1.0 };
        scale_changes(scaled, changes, 4.0, 2.0, 1.0, 0.0, 4.0, 0.0, 4.0);
        REQUIRE(scaled.size() == 2);
        REQUIRE(scaled[0].y == 0.0);
        REQUIRE(scaled[1].y == 4.0);
    }
    SUBCASE("Wrap top") {
        scaled.resize(0);
        changes.back() = std::vector<double> { 0.5, 1.0, 0.5, 1.0 };
        scale_changes(scaled, changes, 4.0, 2.0, 1.0, 0.0, 4.0, 0.0, 4.0);
        REQUIRE(scaled.size() == 2);
        REQUIRE(scaled[0].y == 4.0);
        REQUIRE(scaled[1].y == 0.0);
    }
    SUBCASE("Wrap top left") {
        scaled.resize(0);
        changes.back() = std::vector<double> { 0.0, 1.0, 0.5, 1.0 };
        scale_changes(scaled, changes, 4.0, 2.0, 1.0, 0.0, 4.0, 0.0, 4.0);
        REQUIRE(scaled.size() == 4);
        std::sort(scaled.begin(), scaled.end(), scaled_less);
        REQUIRE(scaled[0].x == 0.0);
        REQUIRE(scaled[0].y == 0.0);
        REQUIRE(scaled[1].x == 0.0);
        REQUIRE(scaled[1].y == 4.0);
        REQUIRE(scaled[2].x == 4.0);
        REQUIRE(scaled[2].y == 0.0);
        REQUIRE(scaled[3].x == 4.0);
        REQUIRE(scaled[3].y == 4.0);
    }
    SUBCASE("Wrap top right") {
        scaled.resize(0);
        changes.back() = std::vector<double> { 0.0, 1.0, 0.5, 1.0 };
        scale_changes(scaled, changes, 4.0, 2.0, 1.0, 0.0, 4.0, 0.0, 4.0);
        REQUIRE(scaled.size() == 4);
        std::sort(scaled.begin(), scaled.end(), scaled_less);
        REQUIRE(scaled[0].x == 0.0);
        REQUIRE(scaled[0].y == 0.0);
        REQUIRE(scaled[1].x == 0.0);
        REQUIRE(scaled[1].y == 4.0);
        REQUIRE(scaled[2].x == 4.0);
        REQUIRE(scaled[2].y == 0.0);
        REQUIRE(scaled[3].x == 4.0);
        REQUIRE(scaled[3].y == 4.0);
    }
    SUBCASE("Wrap bottom right") {
        scaled.resize(0);
        changes.back() = std::vector<double> { 0.0, 0.0, 0.5, 1.0 };
        scale_changes(scaled, changes, 4.0, 2.0, 1.0, 0.0, 4.0, 0.0, 4.0);
        REQUIRE(scaled.size() == 4);
        std::sort(scaled.begin(), scaled.end(), scaled_less);
        REQUIRE(scaled[0].x == 0.0);
        REQUIRE(scaled[0].y == 0.0);
        REQUIRE(scaled[1].x == 0.0);
        REQUIRE(scaled[1].y == 4.0);
        REQUIRE(scaled[2].x == 4.0);
        REQUIRE(scaled[2].y == 0.0);
        REQUIRE(scaled[3].x == 4.0);
        REQUIRE(scaled[3].y == 4.0);
    }
    SUBCASE("Wrap bottom left") {
        scaled.resize(0);
        changes.back() = std::vector<double> { 1.0, 0.0, 0.5, 1.0 };
        scale_changes(scaled, changes, 4.0, 2.0, 1.0, 0.0, 4.0, 0.0, 4.0);
        REQUIRE(scaled.size() == 4);
        std::sort(scaled.begin(), scaled.end(), scaled_less);
        REQUIRE(scaled[0].x == 0.0);
        REQUIRE(scaled[0].y == 0.0);
        REQUIRE(scaled[1].x == 0.0);
        REQUIRE(scaled[1].y == 4.0);
        REQUIRE(scaled[2].x == 4.0);
        REQUIRE(scaled[2].y == 0.0);
        REQUIRE(scaled[3].x == 4.0);
        REQUIRE(scaled[3].y == 4.0);
    }
}

TEST_CASE("row_deltas") {
    std::vector<ScaledChange> scaled(1);
    SUBCASE("Within") {
        scaled.back() = ScaledChange(2.0, 2.0, 0.9, 1.0);
        std::vector<std::int64_t> deltas(5, 0);
        row_deltas(deltas, scaled, 2.0, 4.0, 0.0, 4.0);
        CHECK(deltas[0] == 0);
        CHECK(deltas[1] == 0);
        CHECK(deltas[2] > 0);
        CHECK(deltas[3] < 0);
        CHECK(deltas[4] == 0);
    }
    SUBCASE("Within at limit") {
        scaled.back() = ScaledChange(2.0, 2.0, 1.0, 1.0);
        std::vector<std::int64_t> deltas(5, 0);
        row_deltas(deltas, scaled, 2.0, 4.0, 0.0, 4.0);
        CHECK(deltas[0] == 0);
        CHECK(deltas[1] > 0);
        CHECK(deltas[2] == 0);
        CHECK(deltas[3] == 0);
        CHECK(deltas[4] < 0);
    }
    SUBCASE("Entire row") {
        scaled.back() = ScaledChange(2.0, 2.0, 2.0, 1.0);
        std::vector<std::int64_t> deltas(5, 0);
        row_deltas(deltas, scaled, 2.0, 4.0, 0.0, 4.0);
        CHECK(deltas[0] > 0);
        CHECK(deltas[1] == 0);
        CHECK(deltas[2] == 0);
        CHECK(deltas[3] == 0);
        CHECK(deltas[4] < 0);
    }
    SUBCASE("Left side of range") {
        scaled.back() = ScaledChange(1.0, 2.0, 1.0, 1.0);
        std::vector<std::int64_t> deltas(5, 0);
        row_deltas(deltas, scaled, 2.0, 4.0, 3.0, 4.0);
        CHECK(deltas[0] == 0);
        CHECK(deltas[1] == 0);
        CHECK(deltas[2] == 0);
        CHECK(deltas[3] == 0);
        CHECK(deltas[4] == 0);
    }
    SUBCASE("Right side of range") {
        scaled.back() = ScaledChange(3.0, 2.0, 1.0, 1.0);
        std::vector<std::int64_t> deltas(5, 0);
        row_deltas(deltas, scaled, 2.0, 4.0, 0.0, 1.0);
        CHECK(deltas[0] == 0);
        CHECK(deltas[1] == 0);
        CHECK(deltas[2] == 0);
        CHECK(deltas[3] == 0);
        CHECK(deltas[4] == 0);
    }
}

#endif
