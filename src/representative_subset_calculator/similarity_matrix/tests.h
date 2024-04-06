#include <doctest/doctest.h>
#include <Eigen/Dense>

// TEST_CASE("Testing matrix mutability") {
//     MutableSimilarityMatrix matrix;
//     double previousScore = -1;

//     for (size_t j = 0; j < DATA.size(); j++) {
//         matrix.addRow(DATA[j]);
//         double currentScore = matrix.getCoverage();
//         CHECK(previousScore <= currentScore);
//         previousScore = currentScore;
//     }

//     CHECK(matrix.getCoverage() > 0);
// }

// TEST_CASE("Testing matracies are equivalent") {
//     MutableSimilarityMatrix mutableMatrix(DATA);
//     ImmutableSimilarityMatrix immutableMatrix(DATA);

//     CHECK(mutableMatrix.getCoverage() == immutableMatrix.getCoverage());
// }