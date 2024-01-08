#include <doctest/doctest.h>
#include "similarity_matrix.h"

TEST_CASE("No errors thrown with initialization") {
    SimilarityMatrix matrix(DATA[0]);
    matrix.addRow(DATA[1]);
}