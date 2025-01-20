#include <set>

static const size_t k = DENSE_DATA.size() - 1;
static const float epsilon = 0.01;

static std::unique_ptr<Subset> testCalculator(SubsetCalculator *calculator) {
    std::unique_ptr<FullyLoadedData> data(FullyLoadedData::load(DENSE_DATA));
    NaiveRelevanceCalculator calc(*data);
    std::unique_ptr<Subset> res = calculator->getApproximationSet(calc, *data, k);

    CHECK(res->size() == k);
    std::set<size_t> seen;
    for (const auto & seed : *res.get()) {
        const auto & row = seed;
        CHECK(seen.find(row) == seen.end());
        seen.insert(row);
        CHECK(row >= 0);
        CHECK(row < DENSE_DATA.size());
    }

    delete calculator;
    return move(res);
}

TEST_CASE("Testing Naive representative set finder") {
    testCalculator(new NaiveSubsetCalculator());
}

TEST_CASE("Testing lazy-naive set finder") {
    testCalculator(new LazySubsetCalculator());
}

TEST_CASE("Testing FAST set finder") {
    testCalculator(new FastSubsetCalculator(epsilon));
}

TEST_CASE("Testing LAZYFAST set finder") {
    testCalculator(new LazyFastSubsetCalculator(epsilon));
}

TEST_CASE("All supported calculators have the same result") {
    auto fastRes = testCalculator(new FastSubsetCalculator(epsilon));
    auto lazyFastRes = testCalculator(new LazyFastSubsetCalculator(epsilon));

    checkSolutionsAreEquivalent(*fastRes.get(), *lazyFastRes.get());
}