#include <doctest/doctest.h>

TEST_CASE("Kernel matracies are equivalent") {
    std::unique_ptr<FullyLoadedData> denseData(FullyLoadedData::load(DENSE_DATA));
    std::unique_ptr<NaiveKernelMatrix> naiveMatrix(NaiveKernelMatrix::from(*denseData));
    LazyKernelMatrix lazyMatrix(*denseData);

    for (size_t j = 0; j < denseData->totalRows(); j++) {
        for (size_t i = 0; i < denseData->totalRows(); i++) {
            double naiveValue = naiveMatrix->get(j, i);
            double lazyValue = lazyMatrix.get(j, i);
            CHECK(naiveValue > lazyValue - LARGEST_ACCEPTABLE_ERROR);
            CHECK(naiveValue < lazyValue + LARGEST_ACCEPTABLE_ERROR);
        }
    }
}