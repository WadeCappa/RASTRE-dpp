#include <doctest/doctest.h>

TEST_CASE("Kernel matracies are equivalent") {
    NaiveKernelMatrix naiveMatrix(data);
    LazyKernelMatrix lazyMatrix(data);

    for (size_t j = 0; j < data.rows; j++) {
        for (size_t i = 0; i < data.rows; i++) {
            CHECK(naiveMatrix.get(j, i) == lazyMatrix.get(j, i));
        }
    }
}