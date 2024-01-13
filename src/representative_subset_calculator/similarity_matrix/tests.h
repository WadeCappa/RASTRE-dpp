#include <doctest/doctest.h>
#include <Eigen/Dense>

TEST_CASE("No errors thrown with initialization") {
    SimilarityMatrix matrix(DATA[0]);
    matrix.addRow(DATA[1]);
}

TEST_CASE("Get expected matrix") {
    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> dataMatrix(DATA.size(), DATA[0].size());
    for (size_t j = 0; j < DATA.size(); j++) {
        const auto & row = DATA[j];
        for (size_t i = 0; i < row.size(); i++) {
            const auto & v = row[i];
            dataMatrix(j, i) = v;
        }
    }

    SimilarityMatrix matrix;
    double previousScore = -1;

    for (size_t j = 0; j < DATA.size(); j++) {
        matrix.addRow(DATA[j]);
        auto kernelMatrix = matrix.DEBUG_getMatrix();
        CHECK(kernelMatrix.rows() == kernelMatrix.cols());
        CHECK(kernelMatrix.rows() == j + 1);
        CHECK(kernelMatrix.cols() == j + 1);

        double currentScore = matrix.getCoverage();
        CHECK(previousScore <= currentScore);
        previousScore = currentScore;
    }

    auto lastMatrix = matrix.DEBUG_getMatrix();
    CHECK(lastMatrix == dataMatrix * dataMatrix.transpose());
    CHECK(matrix.getCoverage() > 0);
}