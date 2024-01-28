#include <doctest/doctest.h>

// Do to rounding errors, these results may not always be exactly equivalent
static const double LARGEST_ACCEPTABLE_ERROR = 0.00000001;

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