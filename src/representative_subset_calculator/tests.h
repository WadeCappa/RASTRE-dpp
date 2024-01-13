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
    RepresentativeSubset res = calculator.getApproximationSet(data, k);
    
    CHECK(res.coverage > 0);
    CHECK(res.representativeRows.size() == k);
}