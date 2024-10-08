#include <set>
#include "timers/timers.h"
#include "kernel_matrix/relevance_calculator.h"
#include "kernel_matrix/relevance_calculator_factory.h"
#include "kernel_matrix/kernel_matrix.h"
#include "naive_representative_subset_calculator.h"
#include "lazy_representative_subset_calculator.h"
#include "fast_representative_subset_calculator.h"
#include "lazy_fast_representative_subset_calculator.h"
#include "similarity_matrix/tests.h"

#include "kernel_matrix/tests.h"
#include "orchestrator/tests.h"
#include "buffers/tests.h"
#include "streaming/tests.h"

static const size_t k = DENSE_DATA.size();
static const float epsilon = 0.01;

static std::unique_ptr<Subset> testCalculator(SubsetCalculator *calculator) {
    std::unique_ptr<FullyLoadedData> data(FullyLoadedData::load(DENSE_DATA));
    std::unique_ptr<Subset> res = calculator->getApproximationSet(*data, k);

    CHECK(res->size() == k);
    std::set<size_t> seen;
    for (const auto & seed : *res.get()) {
        const auto & row = seed;
        CHECK(seen.find(row) == seen.end());
        seen.insert(row);
        CHECK(row >= 0);
        CHECK(row < DENSE_DATA.size());
    }

    delete calculator;
    return move(res);
}

TEST_CASE("Testing Naive representative set finder") {
    testCalculator(new NaiveSubsetCalculator());
}

TEST_CASE("Testing lazy-naive set finder") {
    testCalculator(new LazySubsetCalculator());
}

TEST_CASE("Testing FAST set finder") {
    testCalculator(new FastSubsetCalculator(epsilon));
}

TEST_CASE("Testing LAZYFAST set finder") {
    testCalculator(new LazyFastSubsetCalculator(epsilon));
}

TEST_CASE("All calculators have the same result") {
    auto naiveRes = testCalculator(new NaiveSubsetCalculator());
    auto lazyRes = testCalculator(new LazySubsetCalculator());
    auto fastRes = testCalculator(new FastSubsetCalculator(epsilon));
    auto lazyFastRes = testCalculator(new LazyFastSubsetCalculator(epsilon));

    checkSolutionsAreEquivalent(*naiveRes.get(), *lazyRes.get());
    checkSolutionsAreEquivalent(*lazyRes.get(), *fastRes.get());
    checkSolutionsAreEquivalent(*fastRes.get(), *lazyFastRes.get());
}