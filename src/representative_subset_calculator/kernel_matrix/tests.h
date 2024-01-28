#include <doctest/doctest.h>

TEST_CASE("Kernel matracies are equivalent") {
    NaiveKernelMatrix naiveMatrix(data);
    LazyKernelMatrix lazyMatrix(data);

    for (size_t j = 0; j < data.totalRows(); j++) {
        for (size_t i = 0; i < data.totalRows(); i++) {
            double naiveValue = naiveMatrix.get(j, i);
            double lazyValue = lazyMatrix.get(j, i);
            CHECK(naiveValue > lazyValue - LARGEST_ACCEPTABLE_ERROR);
            CHECK(naiveValue < lazyValue + LARGEST_ACCEPTABLE_ERROR);
        }
    }
}