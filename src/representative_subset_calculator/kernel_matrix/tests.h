#include <doctest/doctest.h>

TEST_CASE("Kernel matracies are equivalent") {
    std::unique_ptr<FullyLoadedData> denseData(FullyLoadedData::load(DENSE_DATA));
    NaiveRelevanceCalculator calc(*denseData);
    std::unique_ptr<NaiveKernelMatrix> naiveMatrix(NaiveKernelMatrix::from(*denseData, calc));
    std::unique_ptr<LazyKernelMatrix> lazyMatrix(UnsafeLazyKernelMatrix::from(*denseData, calc));

    for (size_t j = 0; j < denseData->totalRows(); j++) {
        for (size_t i = 0; i < denseData->totalRows(); i++) {
            float naiveValue = naiveMatrix->get(j, i);
            float lazyValue = lazyMatrix->get(j, i);
            CHECK(naiveValue > lazyValue - LARGEST_ACCEPTABLE_ERROR);
            CHECK(naiveValue < lazyValue + LARGEST_ACCEPTABLE_ERROR);
        }
    }
}