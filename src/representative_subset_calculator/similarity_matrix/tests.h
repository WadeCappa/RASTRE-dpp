#include <doctest/doctest.h>
#include <Eigen/Dense>

TEST_CASE("Testing matrix mutability") {
    MutableSimilarityMatrix matrix;
    float previousScore = -1;
    std::unique_ptr<FullyLoadedData> denseData(FullyLoadedData::load(DENSE_DATA));

    for (size_t j = 0; j < denseData->totalRows(); j++) {
        matrix.addRow(denseData->getRow(j));
        float currentScore = matrix.getCoverage();
        CHECK(previousScore <= currentScore);
        previousScore = currentScore;
    }

    CHECK(matrix.getCoverage() > 0);
}