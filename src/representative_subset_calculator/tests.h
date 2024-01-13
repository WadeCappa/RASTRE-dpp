#include "representative_subset_calculator.h"
#include "similarity_matrix/tests.h"

static const Data data{
    DATA,
    DATA.size(),
    DATA[0].size()
};

static const size_t k = 2;

TEST_CASE("Testing Naive representative set finder") {
    NaiveRepresentativeSubsetCalculator calculator;
    std::vector<size_t> res = calculator.getApproximationSet(data, k);

    CHECK(res.size() == k);
}