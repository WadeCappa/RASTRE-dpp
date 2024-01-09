#include <doctest/doctest.h>
#include "similarity_matrix.h"
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

    SimilarityMatrix matrix(DATA[0]);
    auto firstMatrix = matrix.DEBUG_getMatrix();
    CHECK(firstMatrix.rows() == 1);
    CHECK(firstMatrix.cols() == 1);

    for (size_t j = 1; j < DATA.size(); j++) {
        matrix.addRow(DATA[j]);
    }

    auto lastMatrix = matrix.DEBUG_getMatrix();
    CHECK(lastMatrix == dataMatrix.transpose() * dataMatrix);
}