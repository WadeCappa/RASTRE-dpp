#include <set>
#include "timers/timers.h"
#include "naive_representative_subset_calculator.h"
#include "lazy_representative_subset_calculator.h"
#include "fast_representative_subset_calculator.h"
#include "lazy_fast_representative_subset_calculator.h"
#include "similarity_matrix/tests.h"

static const NaiveData data(
    DATA,
    DATA.size(),
    DATA[0].size()
);

#include "kernel_matrix/tests.h"
#include "orchestrator/tests.h"
#include "buffers/tests.h"

static const size_t k = DATA.size();
static const double epsilon = 0.01;

static void testCalculator(SubsetCalculator &calculator) {
    std::unique_ptr<Subset> res = calculator.getApproximationSet(data, k);

    CHECK(res->size() == k);
    std::set<size_t> seen;
    for (const auto & seed : *res.get()) {
        const auto & row = seed;
        CHECK(seen.find(row) == seen.end());
        seen.insert(row);
    }
}

TEST_CASE("Testing Naive representative set finder") {
    NaiveSubsetCalculator calculator;
    testCalculator(calculator);
}

TEST_CASE("Testing lazy-naive set finder") {
    LazySubsetCalculator calculator;
    testCalculator(calculator);
}

TEST_CASE("Testing FAST set finder") {
    FastSubsetCalculator calculator(epsilon);
    testCalculator(calculator);
}

TEST_CASE("Testing LAZYFAST set finder") {
    LazyFastSubsetCalculator calculator(epsilon);
    testCalculator(calculator);
}

void checkSolutionsAreEquivalent(const Subset &a, const Subset &b) {
    CHECK(a.size() == b.size());
    for (size_t i = 0; i < a.size() && i < b.size(); i++) {
        CHECK(a.getRow(i) == b.getRow(i));
    }

    CHECK(a.getScore() > b.getScore() - LARGEST_ACCEPTABLE_ERROR);
    CHECK(a.getScore() < b.getScore() + LARGEST_ACCEPTABLE_ERROR);
}

TEST_CASE("All calculators have the same result") {
    auto naiveRes = NaiveSubsetCalculator().getApproximationSet(data, k);
    auto lazyRes = LazySubsetCalculator().getApproximationSet(data, k);
    auto fastRes = FastSubsetCalculator(epsilon).getApproximationSet(data, k);
    auto lazyFastRes = LazyFastSubsetCalculator(epsilon).getApproximationSet(data, k);

    checkSolutionsAreEquivalent(*naiveRes.get(), *lazyRes.get());
    checkSolutionsAreEquivalent(*lazyRes.get(), *fastRes.get());
    checkSolutionsAreEquivalent(*fastRes.get(), *lazyFastRes.get());
}