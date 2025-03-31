#include "log_macros.h"

#include "user_mode/user_data.h"
#include "representative_subset_calculator/streaming/communication_constants.h"
#include "representative_subset_calculator/representative_subset.h"
#include "data_tools/data_row_visitor.h"
#include "data_tools/to_binary_visitor.h"
#include "data_tools/dot_product_visitor.h"
#include "data_tools/data_row.h"
#include "data_tools/data_row_factory.h"
#include "data_tools/base_data.h"
#include "representative_subset_calculator/timers/timers.h"
#include "data_tools/user_mode_data.h"
#include "representative_subset_calculator/kernel_matrix/relevance_calculator.h"
#include "representative_subset_calculator/kernel_matrix/relevance_calculator_factory.h"
#include "representative_subset_calculator/kernel_matrix/kernel_matrix.h"
#include "representative_subset_calculator/fast_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_fast_representative_subset_calculator.h"
#include "representative_subset_calculator/orchestrator/orchestrator.h"
#include "representative_subset_calculator/memoryProfiler/MemUsage.h"
#include "user_mode/user_score.h"
#include "user_mode/user_subset.h"

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

// TODO: Verify that streaming and lazy-lazy give us the same scores and results on a toy dataset, write test

static const std::vector<std::vector<float>> DENSE_DATA = {
    {4,17,20,1,4,21},
    {5,7,31,45,3,24},
    {2.63212,5.12566,73,15,15,4},
    {12,6,47,32,74,4},
    {1,2,3,4,5,6},
    {6,31,54,3.5,23,57}
};

static const std::vector<std::vector<float>> SPARSE_DATA = {
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

std::vector<std::map<size_t, float>> toMap() {
    std::vector<std::map<size_t, float>> res;
    for (const auto & r : SPARSE_DATA) {
        if (res.size() <= r[0]) {
            res.push_back(std::map<size_t, float>());
        }

        res.back().insert({r[1], r[2]});
    }

    return res;
};

static const std::vector<std::map<size_t, float>> SPARSE_DATA_AS_MAP = toMap();

// Do to rounding errors, these results may not always be exactly equivalent
static const float LARGEST_ACCEPTABLE_ERROR = 0.1;

void checkSolutionsAreEquivalent(const Subset &a, const Subset &b) {
    CHECK(a.size() == b.size());
    for (size_t i = 0; i < a.size() && i < b.size(); i++) {
        CHECK(a.getRow(i) == b.getRow(i));
    }

    CHECK(a.getScore() > b.getScore() - LARGEST_ACCEPTABLE_ERROR);
    CHECK(a.getScore() < b.getScore() + LARGEST_ACCEPTABLE_ERROR);
}

#include "representative_subset_calculator/correctness_comparison_tests.h"
#include "data_tools/tests.h"

#include "representative_subset_calculator/kernel_matrix/tests.h"
#include "representative_subset_calculator/orchestrator/tests.h"
#include "representative_subset_calculator/buffers/tests.h"
#include "representative_subset_calculator/streaming/tests.h"
#include "representative_subset_calculator/user_mode_tests.h"
#include "user_mode/test.h"