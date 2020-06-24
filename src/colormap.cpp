//
//  colormap.cpp
//
//  Created by Ismo Kärkkäinen on 9.6.2020.
//  Copyright © 2020 Ismo Kärkkäinen. All rights reserved.
//
// Licensed under Universal Permissive License. See License.txt.

#include "colormap.hpp"
#include <algorithm>
#if defined(UNITTEST)
#include <doctest/doctest.h>
#endif


static bool front_less(const std::vector<float>& A, const std::vector<float>& B)
{
    return A.front() < B.front();
}

void SortColorMap(std::vector<std::vector<float>>& Map) {
    std::sort(Map.begin(), Map.end(), front_less);
}

static void copy_color(std::vector<float>& Out, const std::vector<float>& Src) {
    for (size_t k = 1; k < Src.size(); ++k)
        Out.push_back(Src[k]);
}

// Requires that value is strictly between end-points, no equality.
static size_t binary_search(float V, const std::vector<std::vector<float>> Map)
{
    size_t low = 0;
    size_t high = Map.size() - 1;
    while (1 < high - low) {
        size_t middle = (high + low) / 2;
        if (Map[middle].front() <= V)
            low = middle;
        else
            high = middle;
    }
    return low;
}

size_t IndexInMap(float V, const std::vector<std::vector<float>>& Map) {
    if (V <= Map.front().front())
        return 0;
    if (Map.back().front() <= V)
        return Map.size() - 1;
    return binary_search(V, Map);
}

static void interpolate(std::vector<float>& Out,
    const float LowWeight, const std::vector<float>& Low,
    const float HighWeight, const std::vector<float>& High)
{
    for (size_t k = 1; k < Low.size(); ++k)
        Out.push_back(LowWeight * Low[k] + HighWeight * High[k]);
}

std::vector<float> Interpolated(
    float V, const std::vector<std::vector<float>>& Map)
{
    std::vector<float> out;
    out.reserve(Map.front().size() - 1);
    if (V <= Map.front().front())
        copy_color(out, Map.front());
    else if (Map.back().front() <= V)
        copy_color(out, Map.back());
    else {
        size_t low = binary_search(V, Map);
        const float range = Map[low + 1].front() - Map[low].front();
        interpolate(out, (Map[low + 1].front() - V) / range, Map[low],
            (V - Map[low].front()) / range, Map[low + 1]);
    }
    return out;
}

#if defined(UNITTEST)

TEST_CASE("front_less") {
    SUBCASE("One") {
        std::vector<float> a = std::vector<float> { 1 };
        std::vector<float> b = std::vector<float> { 2 };
        REQUIRE(front_less(a, b) == true);
        REQUIRE(front_less(b, a) == false);
        REQUIRE(front_less(a, a) == false);
    }
    SUBCASE("Two") {
        std::vector<float> a = std::vector<float> { 1, 2 };
        std::vector<float> b = std::vector<float> { 2, 1 };
        REQUIRE(front_less(a, b) == true);
        REQUIRE(front_less(b, a) == false);
        REQUIRE(front_less(a, a) == false);
    }
}

TEST_CASE("SortColorMap") {
    SUBCASE("One") {
        std::vector<std::vector<float>> v;
        v.push_back(std::vector<float> { 1.0f });
        SortColorMap(v);
        REQUIRE(v.front().front() == 1.0f);
    }
    SUBCASE("Two") {
        std::vector<std::vector<float>> v;
        v.push_back(std::vector<float> { 2.0f });
        v.push_back(std::vector<float> { 1.0f });
        SortColorMap(v);
        REQUIRE(v.front().front() == 1.0f);
        REQUIRE(v.back().front() == 2.0f);
    }
}

TEST_CASE("copy_color") {
    SUBCASE("Zero") {
        std::vector<float> a = std::vector<float> { 1 };
        std::vector<float> b;
        copy_color(b, a);
        REQUIRE(b.empty());
    }
    SUBCASE("One") {
        std::vector<float> a = std::vector<float> { 1.0f, 2.0f };
        std::vector<float> b;
        copy_color(b, a);
        REQUIRE(b.size() == a.size() - 1);
        REQUIRE(b.front() == a.back());
    }
    SUBCASE("Three") {
        std::vector<float> a = std::vector<float> { 1.0f, 2.0f, 3.0f, 4.0f };
        std::vector<float> b;
        copy_color(b, a);
        REQUIRE(b.size() == a.size() - 1);
        for (size_t k = 0; k < b.size(); ++k)
            REQUIRE(b[k] == a[k + 1]);
    }
}

TEST_CASE("binary_search") {
    std::vector<std::vector<float>> map;
    map.push_back(std::vector<float> { 0.0f, 0.0f, 0.0f, 0.0f });
    map.push_back(std::vector<float> { 1.0f, 1.0f, 1.0f, 1.0f });
    map.push_back(std::vector<float> { 2.0f, 2.0f, 2.0f, 2.0f });
    map.push_back(std::vector<float> { 3.0f, 3.0f, 3.0f, 3.0f });
    map.push_back(std::vector<float> { 4.0f, 4.0f, 4.0f, 4.0f });
    map.push_back(std::vector<float> { 5.0f, 5.0f, 5.0f, 5.0f });
    map.push_back(std::vector<float> { 6.0f, 6.0f, 6.0f, 6.0f });
    SUBCASE("Just past value") {
        for (size_t k = 0; k < map.size() - 1; ++k)
            REQUIRE(binary_search(k + 0.125f, map) == k);
    }
}

TEST_CASE("interpolate") {
    std::vector<float> low = std::vector<float> { 0.0f, 0.0f, 0.0f, 0.0f };
    std::vector<float> high = std::vector<float> { 1.0f, 2.0f, 3.0f, 4.0f };
    std::vector<float> out;
    SUBCASE("Low") {
        out.resize(0);
        interpolate(out, 1.0f, low, 0.0f, high);
        REQUIRE(out.size() == low.size() - 1);
        for (size_t k = 0; k < out.size(); ++k)
            REQUIRE(out[k] == low[k + 1]);
    }
    SUBCASE("High") {
        out.resize(0);
        interpolate(out, 0.0f, low, 1.0f, high);
        REQUIRE(out.size() == high.size() - 1);
        for (size_t k = 0; k < out.size(); ++k)
            REQUIRE(out[k] == high[k + 1]);
    }
    SUBCASE("Middle") {
        out.resize(0);
        interpolate(out, 0.5f, low, 0.5f, high);
        REQUIRE(out.size() == low.size() - 1);
        for (size_t k = 0; k < out.size(); ++k)
            REQUIRE(out[k] == 0.5f * (low[k + 1] + high[k + 1]));
    }
}

#endif

