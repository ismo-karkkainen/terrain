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
#include "convenience.hpp"
#endif
#include "generate_io.hpp"
#include <vector>
#include <iostream>
#include <cmath>
#include <cinttypes>
#include <functional>
#include <random>
#include <tuple>
#include <fcntl.h>
#include <unistd.h>


static std::mt19937_64 rnd;

typedef std::function<double(double,double,double)> Mapper;

static double value(double x, double y, io::GenerateIn::radius_minType& Map) {
    y *= Map.size() - 2;
    std::size_t yidx = trunc(y);
    y -= yidx;
    double xx = x * (Map[yidx].size() - 2);
    std::size_t xidx = trunc(xx);
    xx -= xidx;
    double v = (1.0 - y) * ((1.0 - xx) * Map[yidx][xidx] + xx * Map[yidx][xidx + 1]);
    ++yidx;
    xx = x * (Map[yidx].size() - 2);
    xidx = trunc(xx);
    xx -= xidx;
    return v + y * ((1.0 - xx) * Map[yidx][xidx] + xx * Map[yidx][xidx + 1]);
}

static void generate_change(
    std::vector<double>& Change, Mapper Radius, Mapper Offset)
{
    Change[2] = Radius(Change[0], Change[1], Change[2]);
    Change[3] = Offset(Change[0], Change[1], Change[3]);
}

static double minmax(double x, double y, double r,
    io::GenerateIn::radius_minType& a, io::GenerateIn::radius_minType& b)
{
    double v = value(x, y, a);
    return v + (value(x, y, b) - v) * r;
}

static double minrange(double x, double y, double r,
    io::GenerateIn::radius_minType& a, io::GenerateIn::radius_minType& b)
{
    return value(x, y, a) + value(x, y, b) * r;
}

static double maxrange(double x, double y, double r,
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

static void check_histogram(
    bool Given, io::GenerateIn::offset_histogramType& Value)
{
    if (Given) {
        if (Value.empty())
            throw "Input histogram is empty.";
        bool nonzero = false;
        for (auto& v : Value) {
            if (v < 0.0f)
                throw "Input histogram has negative value.";
            if (0.0f < v)
                nonzero = true;
        }
        if (!nonzero)
            throw "Input histogram has only zeroes.";
    } else
        Value.push_back(1.0f);
}

static void normalize_histogram(io::GenerateIn::offset_histogramType& Hist) {
    double sum = 0.0;
    for (std::size_t k = 0; k < Hist.size(); ++k)
        sum += Hist[k];
    for (std::size_t k = 0; k < Hist.size(); ++k)
        Hist[k] /= sum;
}

// Old low, high, and range, new low and range.
typedef std::vector<std::tuple<double,double,double,double,double>> RangeMap;

static RangeMap histogram2rangemap(
    const io::GenerateIn::offset_histogramType& Hist)
{
    RangeMap map;
    double previous = 0.0;
    for (std::size_t k = 0; k < Hist.size(); ++k) {
        if (Hist[k] == 0.0) // Skip empty ranges.
            continue;
        map.push_back(std::tuple(previous, previous + Hist[k], double(Hist[k]),
            double(k) / double(Hist.size()), 1.0 / double(Hist.size())));
        previous += Hist[k];
    }
    // Ensure inequality will hold in redistribute.
    std::get<1>(map.back()) = 1.0 + std::numeric_limits<double>::epsilon();
    return map;
}

static double redistribute(double Value, const RangeMap Map) {
    // A binary search would be more efficient once histogram is long enough.
    for (auto& interval : Map) {
        if (std::get<0>(interval) <= Value && Value < std::get<1>(interval))
            return std::get<3>(interval) + std::get<4>(interval) *
                ((Value - std::get<0>(interval)) / std::get<2>(interval));
    }
    return std::get<3>(Map.back()) + std::get<4>(Map.back());
}

#if !defined(UNITTEST)

static int generate(io::GenerateIn& Val) {
    std::vector<char> output_buffer;
    const double s = 1.0 / static_cast<double>(std::mt19937_64::max());
    std::vector<double> change(4, 0.0);
    try {
        check_map(Val.radius_minGiven(), Val.radius_min(), 0.0f);
        check_map(Val.radius_maxGiven(), Val.radius_max(), 1.0f);
        check_map(Val.radius_rangeGiven(), Val.radius_range(), 1.0f);
        check_map(Val.offset_minGiven(), Val.offset_min(), -1.0f);
        check_map(Val.offset_maxGiven(), Val.offset_max(), 1.0f);
        check_map(Val.offset_rangeGiven(), Val.offset_range(), 2.0f);
        check_histogram(Val.offset_histogramGiven(), Val.offset_histogram());
        check_histogram(Val.radius_histogramGiven(), Val.radius_histogram());
    }
    catch (const char* msg) {
        std::cerr << msg << std::endl;
        return 2;
    }
    if (Val.seedGiven())
        rnd.seed(Val.seed());
    Mapper radius = [&Val](double x, double y, double r) {
        return minrange(x, y, r, Val.radius_min(), Val.radius_range());
    };
    if (Val.radius_minGiven() && Val.radius_maxGiven()) {
        radius = [&Val](double x, double y, double r) {
            return minmax(x, y, r, Val.radius_min(), Val.radius_max());
        };
    } else if (Val.radius_maxGiven()) {
        radius = [&Val](double x, double y, double r) {
            return maxrange(x, y, r, Val.radius_max(), Val.radius_range());
        };
    }
    Mapper offset = [&Val](double x, double y, double r) {
        return minrange(x, y, r, Val.offset_min(), Val.offset_range());
    };
    if (Val.offset_minGiven() && Val.offset_maxGiven()) {
        offset = [&Val](double x, double y, double r) {
            return minmax(x, y, r, Val.offset_min(), Val.offset_max());
        };
    } else if (Val.offset_maxGiven()) {
        offset = [&Val](double x, double y, double r) {
            return maxrange(x, y, r, Val.offset_max(), Val.offset_range());
        };
    }
    normalize_histogram(Val.offset_histogram());
    RangeMap offset_map = histogram2rangemap(Val.offset_histogram());
    normalize_histogram(Val.radius_histogram());
    RangeMap radius_map = histogram2rangemap(Val.radius_histogram());
    std::cout << "{\"changes\":[";
    for (std::uint32_t k = 0; k < Val.count(); ++k) {
        for (auto& v : change)
            v = s * static_cast<double>(rnd());
        change[2] = redistribute(change[2], radius_map);
        change[3] = redistribute(change[3], offset_map);
        generate_change(change, radius, offset);
        io::Write(std::cout, change, output_buffer);
        if (k + 1 != Val.count())
            std::cout << ',';
    }
    std::cout << "]}" << std::endl;
    return 0;
}

int main(int argc, char** argv) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    rnd.seed((static_cast<std::mt19937_64::result_type>(getpid()) << 17) ^
        (static_cast<std::mt19937_64::result_type>(ts.tv_nsec) << 34) ^
        static_cast<std::mt19937_64::result_type>(ts.tv_sec));
    int f = 0;
    if (argc > 1)
        f = open(argv[1], O_RDONLY);
    InputParser<io::ParserPool, io::GenerateIn_Parser, io::GenerateIn> ip(f);
    int status = ip.ReadAndParse(generate);
    if (f)
        close(f);
    return status;
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
                REQUIRE(val == 0.5);
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
                REQUIRE(val == 0.5);
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
        REQUIRE(abs(value(0.0, 0.0, map) - 1.0) < 1e-7);
        REQUIRE(abs(value(1.0, 0.0, map) - 2.0) < 1e-7);
        REQUIRE(abs(value(0.0, 1.0, map) - 3.0) < 1e-7);
        REQUIRE(abs(value(1.0, 1.0, map) - 4.0) < 1e-7);
    }
    SUBCASE("Edge centers") {
        REQUIRE(abs(value(0.5, 0.0, map) - 1.5) < 1e-7);
        REQUIRE(abs(value(1.0, 0.5, map) - 3.0) < 1e-7);
        REQUIRE(abs(value(0.5, 1.0, map) - 3.5) < 1e-7);
        REQUIRE(abs(value(0.0, 0.5, map) - 2.0) < 1e-7);
    }
    SUBCASE("Center") {
        REQUIRE(abs(value(0.5, 0.5, map) - 2.5) < 1e-7);
    }
}

TEST_CASE("generate_change") {
    std::vector<double> change(4, 1.0);
    SUBCASE("Touch correct") {
        generate_change(change,
            [](double x, double y, double r) { return 2.0; },
            [](double x, double y, double r) { return 3.0; });
        REQUIRE(change[0] == 1.0);
        REQUIRE(change[1] == 1.0);
        REQUIRE(change[2] == 2.0);
        REQUIRE(change[3] == 3.0);
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
        REQUIRE(minmax(0.0, 0.0, 0.5, map, map) == 1.0);
        REQUIRE(minmax(0.0, 0.0, 0.0, map, map) == 1.0);
        REQUIRE(minmax(0.0, 0.0, 1.0, map, map) == 1.0);
    }
    io::GenerateIn::radius_minType map2;
    map2.push_back(std::vector<float>(2, 2.0f));
    map2.back().back() = 3.0f;
    map2.push_back(std::vector<float>(2, 4.0f));
    map2.back().back() = 5.0f;
    check_map(true, map2, 0.0f);
    SUBCASE("min + 1 == max") {
        REQUIRE(minmax(0.0, 0.0, 0.5, map, map2) == 1.5);
        REQUIRE(minmax(0.0, 0.0, 0.0, map, map2) == 1.0);
        REQUIRE(minmax(0.0, 0.0, 1.0, map, map2) == 2.0);
    }
}

TEST_CASE("minrange") {
    io::GenerateIn::radius_minType map;
    map.push_back(std::vector<float>(1, 1.0f));
    check_map(true, map, 0.0f);
    SUBCASE("min == range") {
        REQUIRE(minrange(0.0, 0.0, 0.5, map, map) == 1.5);
        REQUIRE(minrange(0.0, 0.0, 0.0, map, map) == 1.0);
        REQUIRE(minrange(0.0, 0.0, 1.0, map, map) == 2.0);
    }
}

TEST_CASE("maxrange") {
    io::GenerateIn::radius_minType map;
    map.push_back(std::vector<float>(1, 1.0f));
    check_map(true, map, 0.0f);
    SUBCASE("max == range") {
        REQUIRE(maxrange(0.0, 0.0, 0.5, map, map) == 0.5 * map[0][0]);
        REQUIRE(maxrange(0.0, 0.0, 0.0, map, map) == map[0][0]);
        REQUIRE(maxrange(0.0, 0.0, 1.0, map, map) == 0.0);
    }
}

TEST_CASE("check_histogram") {
    io::GenerateIn::offset_histogramType hist;
    SUBCASE("Empty given") {
        REQUIRE_THROWS_AS(check_histogram(true, hist), const char*);
    }
    SUBCASE("Not given") {
        check_histogram(false, hist);
        REQUIRE(hist.size() == 1);
        REQUIRE(hist.front() == 1.0f);
    }
    SUBCASE("Zero only") {
        hist.resize(0);
        hist.push_back(0.0f);
        REQUIRE_THROWS_AS(check_histogram(true, hist), const char*);
    }
    SUBCASE("Valid given") {
        hist.resize(0);
        hist.push_back(1.0f);
        hist.push_back(0.5f);
        check_histogram(true, hist);
        REQUIRE(hist.size() == 2);
        REQUIRE(hist.front() == 1.0f);
        REQUIRE(hist.back() == 0.5f);
    }
    SUBCASE("Negative value") {
        hist.resize(0);
        hist.push_back(0.0f);
        hist.push_back(0.5f);
        hist.push_back(-1.0f);
        REQUIRE_THROWS_AS(check_histogram(true, hist), const char*);
    }
}

TEST_CASE("normalize_histogram") {
    SUBCASE("Simple") {
        io::GenerateIn::offset_histogramType hist;
        hist.push_back(2.0f);
        normalize_histogram(hist);
        REQUIRE(hist.front() == 1.0f);
    }
    SUBCASE("Uniform") {
        io::GenerateIn::offset_histogramType hist =
            std::vector<float> { 2.0f, 2.0f, 2.0f, 2.0f };
        normalize_histogram(hist);
        for (auto& v : hist)
            REQUIRE(v == 0.25f);
    }
    SUBCASE("Peaks") {
        io::GenerateIn::offset_histogramType hist =
            std::vector<float> { 2.0f, 0.0f, 2.0f };
        normalize_histogram(hist);
        REQUIRE(hist.front() == 0.5f);
        REQUIRE(hist[1] == 0.0f);
        REQUIRE(hist.back() == 0.5f);
    }
}

TEST_CASE("histogram2rangemap") {
    SUBCASE("Uniform") {
        io::GenerateIn::offset_histogramType hist;
        hist.push_back(1.0f);
        normalize_histogram(hist);
        RangeMap map = histogram2rangemap(hist);
        REQUIRE(map.size() == 1);
        REQUIRE(std::get<0>(map.front()) == 0.0);
        REQUIRE(std::get<1>(map.front()) >= 1.0);
        REQUIRE(std::get<2>(map.front()) == 1.0);
        REQUIRE(std::get<3>(map.front()) == 0.0);
        REQUIRE(std::get<4>(map.front()) == 1.0);
    }
    SUBCASE("Peaks") {
        io::GenerateIn::offset_histogramType hist =
            std::vector<float> { 1.0f, 0.0f, 0.0f, 1.0f };
        normalize_histogram(hist);
        RangeMap map = histogram2rangemap(hist);
        REQUIRE(map.size() == 2);
        REQUIRE(std::get<0>(map.front()) == 0.0);
        REQUIRE(std::get<1>(map.front()) == 0.5);
        REQUIRE(std::get<2>(map.front()) == 0.5);
        REQUIRE(std::get<3>(map.front()) == 0.0);
        REQUIRE(std::get<4>(map.front()) == 0.25);
        REQUIRE(std::get<0>(map.back()) == 0.5);
        REQUIRE(std::get<1>(map.back()) >= 1.0);
        REQUIRE(std::get<2>(map.back()) == 0.5);
        REQUIRE(std::get<3>(map.back()) == 0.75);
        REQUIRE(std::get<4>(map.back()) == 0.25);
    }
}

TEST_CASE("redistribute") {
    SUBCASE("Uniform") {
        io::GenerateIn::offset_histogramType hist = std::vector<float> { 1.0f };
        normalize_histogram(hist);
        RangeMap map = histogram2rangemap(hist);
        REQUIRE(redistribute(0.0, map) == 0.0);
        REQUIRE(redistribute(0.25, map) == 0.25);
        REQUIRE(redistribute(0.5, map) == 0.5);
        REQUIRE(redistribute(0.75, map) == 0.75);
        REQUIRE(redistribute(1.0, map) == 1.0);
    }
    SUBCASE("Peaks") {
        io::GenerateIn::offset_histogramType hist =
            std::vector<float> { 1.0f, 0.0f, 0.0f, 1.0f };
        normalize_histogram(hist);
        RangeMap map = histogram2rangemap(hist);
        REQUIRE(redistribute(0.0, map) == 0.0);
        REQUIRE(redistribute(0.499, map) >= 0.248);
        REQUIRE(redistribute(0.499, map) <= 0.25);
        REQUIRE(redistribute(0.5, map) == 0.75);
        REQUIRE(redistribute(1.0, map) == 1.0);
    }
    SUBCASE("Upper half") {
        io::GenerateIn::offset_histogramType hist =
            std::vector<float> { 0.0f, 1.0f };
        normalize_histogram(hist);
        RangeMap map = histogram2rangemap(hist);
        REQUIRE(redistribute(0.0, map) == 0.5);
        REQUIRE(redistribute(0.5, map) == 0.75);
        REQUIRE(redistribute(1.0, map) == 1.0);
    }
}

#endif
