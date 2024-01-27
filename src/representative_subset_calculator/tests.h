#include <set>
#include "naive_representative_subset_calculator.h"
#include "lazy_representative_subset_calculator.h"
#include "fast_representative_subset_calculator.h"
#include "lazy_fast_representative_subset_calculator.h"
#include "similarity_matrix/tests.h"

static const Data data(
    DATA,
    DATA.size(),
    DATA[0].size()
);

#include "kernel_matrix/tests.h"

static const size_t k = DATA.size();

static const double epsilon = 0.01;

TEST_CASE("Testing Naive representative set finder") {
    Timers timers;
    NaiveRepresentativeSubsetCalculator calculator(timers);
    std::vector<std::pair<size_t, double>> res = calculator.getApproximationSet(data, k);
    
    CHECK(res.size() == k);

    std::set<size_t> seen;
    for (const auto & seed : res) {
        const auto & row = seed.first;
        CHECK(seen.find(row) == seen.end());
        seen.insert(row);
    }
}

TEST_CASE("Testing lazy-naive set finder") {
    Timers timers;
    LazyRepresentativeSubsetCalculator calculator(timers);
    std::vector<std::pair<size_t, double>> res = calculator.getApproximationSet(data, k);

    CHECK(res.size() == k);

    std::set<size_t> seen;
    for (const auto & seed : res) {
        const auto & row = seed.first;
        CHECK(seen.find(row) == seen.end());
        seen.insert(row);
    }
}

TEST_CASE("Testing FAST set finder") {
    Timers timers;
    FastRepresentativeSubsetCalculator calculator(timers, epsilon);
    std::vector<std::pair<size_t, double>> res = calculator.getApproximationSet(data, k);

    CHECK(res.size() == k);

    std::set<size_t> seen;
    for (const auto & seed : res) {
        const auto & row = seed.first;
        CHECK(seen.find(row) == seen.end());
        seen.insert(row);
    }
}

TEST_CASE("Testing LAZYFAST set finder") {
    Timers timers;
    LazyFastRepresentativeSubsetCalculator calculator(timers, epsilon);
    std::vector<std::pair<size_t, double>> res = calculator.getApproximationSet(data, k);

    CHECK(res.size() == k);

    std::set<size_t> seen;
    for (const auto & seed : res) {
        const auto & row = seed.first;
        CHECK(seen.find(row) == seen.end());
        seen.insert(row);
    }
}