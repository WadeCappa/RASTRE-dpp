
static std::unique_ptr<Subset> testCalculator(
    SubsetCalculator *calculator, 
    std::unique_ptr<RelevanceCalculator> relevanceCalculator,
    const BaseData &data,
    std::unique_ptr<MutableSubset> consumer, 
    const size_t k,
    const float epsilon) {
    std::unique_ptr<Subset> res = calculator->getApproximationSet(move(consumer), *relevanceCalculator, data, k);

    CHECK(res->size() == k);
    std::set<size_t> seen;
    for (const size_t seed : *res) {
        CHECK(seen.find(seed) == seen.end());
        seen.insert(seed);
        CHECK(seed >= 0);
        CHECK(seed < DENSE_DATA.size());
    }

    delete calculator;
    return std::move(res);
}

static std::unique_ptr<Subset> testCalculator(
    SubsetCalculator *calculator, 
    const BaseData &data,
    const size_t k,
    const float epsilon) {
    return testCalculator(calculator, NaiveRelevanceCalculator::from(data), data, NaiveMutableSubset::makeNew(), k, epsilon);
}

TEST_CASE("Testing FAST set finder") {
    std::unique_ptr<FullyLoadedData> data(FullyLoadedData::load(DENSE_DATA));
    const size_t k = DENSE_DATA.size() - 1;
    const float epsilon = 0.01;
    testCalculator(new FastSubsetCalculator(epsilon), *data, k, epsilon);
}

TEST_CASE("Testing LAZYFAST set finder") {
    std::unique_ptr<FullyLoadedData> data(FullyLoadedData::load(DENSE_DATA));
    const size_t k = DENSE_DATA.size() - 1;
    const float epsilon = 0.01;
    testCalculator(new LazyFastSubsetCalculator(epsilon), *data, k, epsilon);
}

TEST_CASE("All supported calculators have the same result") {
    std::unique_ptr<FullyLoadedData> data(FullyLoadedData::load(DENSE_DATA));
    const size_t k = DENSE_DATA.size() - 1;
    const float epsilon = 0.01;
    auto fastRes = testCalculator(new FastSubsetCalculator(epsilon), *data, k, epsilon);
    auto lazyFastRes = testCalculator(new LazyFastSubsetCalculator(epsilon), *data, k, epsilon);

    checkSolutionsAreEquivalent(*fastRes.get(), *lazyFastRes.get());
}
