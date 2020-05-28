//
//  render-changes.cpp
//
//  Created by Ismo Kärkkäinen on 17.5.2020.
//  Copyright © 2020 Ismo Kärkkäinen. All rights reserved.
//
// Licensed under Universal Permissive License. See License.txt.

#include "FileDescriptorInput.hpp"
#include "BlockQueue.hpp"
#include "render_io.hpp"
#include <vector>
#include <iostream>
#include <cmath>
#include <ctime>
#include <cinttypes>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>


static void render_changes(io::RenderChangesIn& Val) {
    const std::uint32_t size = Val.size();
    const std::uint32_t low = Val.lowGiven() ? std::min(Val.low(), size) : 0;
    const std::uint32_t high = Val.highGiven() ? std::min(Val.high(), size) : size;
    const std::uint32_t left = Val.leftGiven() ? std::min(Val.left(), size) : 0;
    const std::uint32_t right = Val.rightGiven() ? std::min(Val.right(), size) : size;
    if (high <= low)
        return;
    const float max_radius = 0.5f * Val.size();
    std::vector<char> buffer;
    std::vector<float> deltas(size + 1, 0.0f);
    std::vector<float> row;
    if (left < right)
        row.resize(right - left);
    for (std::uint32_t y = low; y < high; ++y) {
        if (left < right) {
            for (auto& change : Val.changes()) {
                const float cy = trunc(change[1] * size);
                const float radius = change[2] * max_radius;
                const float diff = fabs(cy - y);
                if (radius < diff) {
                    if (cy < y) {
                        if (cy + radius < y || y < cy + size - radius)
                            continue;
                    } else {
                        if (y + radius < cy || cy < y + size - radius)
                            continue;
                    }
                }
                const float cx = change[0] * size;
                const float line_span = sqrtf(radius * radius - diff * diff);
                float from = round(cx - line_span);
                float to = round(cx + line_span);
                if (0 < from && right < from)
                    continue;
                if (to < size && to < left)
                    continue;
                // Wraps around.
                if (from < 0 && right < from + size)
                    continue;
                if (size <= to && left < to - size)
                    continue;
                if (0.0f <= from && to < size) {
                    deltas[trunc(from)] += change[3];
                    deltas[trunc(to)] -= change[3];
                } else {
                    if (from < 0)
                        from += size;
                    else
                        to -= size;
                    deltas.front() += change[3];
                    deltas[to + 1] -= change[3];
                    deltas[from] += change[3];
                }
            }
        }
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
