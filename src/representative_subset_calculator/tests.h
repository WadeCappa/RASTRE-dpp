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

static void testCalculator(RepresentativeSubsetCalculator &calculator) {
    std::unique_ptr<RepresentativeSubset> res = calculator.getApproximationSet(data, k);

    CHECK(res->size() == k);
    std::set<size_t> seen;
    for (const auto & seed : *res.get()) {
        const auto & row = seed;
        CHECK(seen.find(row) == seen.end());
        seen.insert(row);
    }
}

TEST_CASE("Testing Naive representative set finder") {
    NaiveRepresentativeSubsetCalculator calculator;
    testCalculator(calculator);
}

TEST_CASE("Testing lazy-naive set finder") {
    LazyRepresentativeSubsetCalculator calculator;
    testCalculator(calculator);
}

TEST_CASE("Testing FAST set finder") {
    FastRepresentativeSubsetCalculator calculator(epsilon);
    testCalculator(calculator);
}

TEST_CASE("Testing LAZYFAST set finder") {
    LazyFastRepresentativeSubsetCalculator calculator(epsilon);
    testCalculator(calculator);
}

void checkSolutionsAreEquivalent(const RepresentativeSubset &a, const RepresentativeSubset &b) {
    CHECK(a.size() == b.size());
    for (size_t i = 0; i < a.size() && i < b.size(); i++) {
        CHECK(a.getRow(i) == b.getRow(i));
    }

    CHECK(a.getScore() > b.getScore() - LARGEST_ACCEPTABLE_ERROR);
    CHECK(a.getScore() < b.getScore() + LARGEST_ACCEPTABLE_ERROR);
}

TEST_CASE("All calculators have the same result") {
    auto naiveRes = NaiveRepresentativeSubsetCalculator().getApproximationSet(data, k);
    auto lazyRes = LazyRepresentativeSubsetCalculator().getApproximationSet(data, k);
    auto fastRes = FastRepresentativeSubsetCalculator(epsilon).getApproximationSet(data, k);
    auto lazyFastRes = LazyFastRepresentativeSubsetCalculator(epsilon).getApproximationSet(data, k);

    checkSolutionsAreEquivalent(*naiveRes.get(), *lazyRes.get());
    checkSolutionsAreEquivalent(*lazyRes.get(), *fastRes.get());
    checkSolutionsAreEquivalent(*fastRes.get(), *lazyFastRes.get());
}