#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "representative_subset_calculator/streaming/communication_constants.h"

static const std::vector<std::vector<double>> DENSE_DATA = {
    {4,17,20,1,4,21},
    {5,7,31,45,3,24},
    {2.63212,5.12566,73,15,15,4},
    {12,6,47,32,74,4},
    {1,2,3,4,5,6},
    {6,31,54,3.5,23,57}
};

static const std::vector<std::vector<double>> SPARSE_DATA = {
    {0, 1, 4.2},
    {0, 2, 32},
    {0, 4, 1.23},
    {1, 4, 2.12},
    {1, 5, 2.521},
    {2, 1, 321},
    {2, 3, 2.3},
    {2, 4, 32.1},
    {3, 2, 124},
    {3, 5, 1.52},
    {4, 4, 212.32},
    {5, 2, 312.32}
};

size_t totalColumns() {
    size_t largestColumn = 0;
    for (const auto & r : SPARSE_DATA) {
        if (largestColumn < r[1]) {
            largestColumn = r[1];
        }
    }

    return largestColumn;
};

static const size_t SPARSE_DATA_TOTAL_COLUMNS = totalColumns();

std::vector<std::map<size_t, double>> toMap() {
    std::vector<std::map<size_t, double>> res;
    for (const auto & r : SPARSE_DATA) {
        if (res.size() <= r[0]) {
            res.push_back(std::map<size_t, double>());
        }

        res.back().insert({r[1], r[2]});
    }

    return res;
};

static const std::vector<std::map<size_t, double>> SPARSE_DATA_AS_MAP = toMap();

// Do to rounding errors, these results may not always be exactly equivalent
static const double LARGEST_ACCEPTABLE_ERROR = 0.00000001;

#include "data_tools/tests.h"

#include "representative_subset_calculator/tests.h"