#include <set>
#include "naive_representative_subset_calculator.h"
#include "lazy_representative_subset_calculator.h"
#include "similarity_matrix/tests.h"

static const Data data{
    DATA,
    DATA.size(),
    DATA[0].size()
};

static const size_t k = DATA.size();

TEST_CASE("Testing Naive representative set finder") {
    Timers timers;
    NaiveRepresentativeSubsetCalculator calculator(timers);
    RepresentativeSubset res = calculator.getApproximationSet(data, k);
    
    CHECK(res.coverage > 0);
    CHECK(res.representativeRows.size() == k);

    std::set<size_t> seen;
    for (const auto & row : res.representativeRows) {
        CHECK(seen.find(row) == seen.end());
        seen.insert(row);
    }
}

TEST_CASE("Testing lazy-naive set finder") {
    Timers timers;
    LazyRepresentativeSubsetCalculator calculator(timers);
    RepresentativeSubset res = calculator.getApproximationSet(data, k);

    CHECK(res.coverage > 0);
    CHECK(res.representativeRows.size() == k);

    std::set<size_t> seen;
    for (const auto & row : res.representativeRows) {
        CHECK(seen.find(row) == seen.end());
        seen.insert(row);
    }
}