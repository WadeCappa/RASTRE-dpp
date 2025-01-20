TEST_CASE("test decorator mappings") {
    std::unique_ptr<UserData> userData(UserDataImplementation::from(
        11, 
        5, 
        std::vector<unsigned long long>({0, 4, 5}), 
        std::vector<double>({2.1245, 1.125, 1.43123})));
    std::unique_ptr<FullyLoadedData> data(FullyLoadedData::load(DENSE_DATA));
    UserModeDataDecorator decorator(*data, *userData);
    CHECK(decorator.totalRows() == userData->getCu().size());
    for (size_t r = 0; r < decorator.totalRows(); r++) {
        CHECK(decorator.getRemoteIndexForRow(r) == userData->getCu()[r]);
    }
}

TEST_CASE("testing segmented data + user_mode data") {
    std::unique_ptr<UserData> userData(UserDataImplementation::from(
        11, 
        5, 
        std::vector<unsigned long long>({0, 4}), 
        std::vector<double>({2.1245, 1.125})));
    std::vector<std::unique_ptr<DataRow>> data;
    
    // only grab every other row
    for (size_t i = 0; i < DENSE_DATA.size(); i+=2) {
        std::vector<float> v(DENSE_DATA[i]);
        data.push_back(std::unique_ptr<DataRow>(new DenseDataRow(move(v))));
    }

    LoadedSegmentedData segmentedData(move(data), std::vector<size_t>({0, 2, 4}), DENSE_DATA[0].size());
    UserModeDataDecorator decorator(segmentedData, *userData);

    CHECK(decorator.totalRows() == userData->getCu().size());
    for (size_t r = 0; r < decorator.totalRows(); r++) {
        CHECK(decorator.getRemoteIndexForRow(r) == userData->getCu()[r]);
    }
}

TEST_CASE("testing user-mode relevance calculator provides valid results") {
    std::unique_ptr<UserData> userData(UserDataImplementation::from(
        11, 
        5, 
        std::vector<unsigned long long>({0, 4, 5}), 
        std::vector<double>({2.1245, 1.125, 1.43123})));
    std::unique_ptr<FullyLoadedData> data(FullyLoadedData::load(DENSE_DATA));
    UserModeDataDecorator decorator(*data, *userData);
    const double theta = 0.7;
    std::unique_ptr<UserModeRelevanceCalculator> calc(
        UserModeRelevanceCalculator::from(decorator, *userData, theta)
    );
    for (size_t j = 0; j < decorator.totalRows(); j++) {
        for (size_t i = 0; i < decorator.totalRows(); i++) {
            CHECK(calc->get(i, j) > 0);
        }
    }
}

TEST_CASE("testing translating subset") {
    std::unique_ptr<UserData> userData(UserDataImplementation::from(
        11, 
        5, 
        std::vector<unsigned long long>({0, 4, 5}), 
        std::vector<double>({2.1245, 1.125, 1.43123})));
    std::unique_ptr<FullyLoadedData> data(FullyLoadedData::load(DENSE_DATA));
    UserModeDataDecorator decorator(*data, *userData);
    std::unique_ptr<TranslatingUserSubset> consumer(
        TranslatingUserSubset::create(decorator)
    );

    for (size_t r = 0; r < decorator.totalRows(); r++) {
        consumer->addRow(r, 5.0);
    }

    /**
     * The consumer should contain global row indicies, not rows scoped to a user
     */
    for (size_t r = 0; r < decorator.totalRows(); r++) {
        const size_t row_from_data = decorator.getRemoteIndexForRow(r);
        const size_t from_consumer = consumer->getRow(r);
        CHECK(row_from_data == from_consumer);
    }
}

TEST_CASE("testing kernel matricies with user-mode classes") {
    std::unique_ptr<UserData> userData(UserDataImplementation::from(
        11, 
        5, 
        std::vector<unsigned long long>({0, 4, 5}), 
        std::vector<double>({2.1245, 1.125, 1.43123})));
    std::unique_ptr<FullyLoadedData> data(FullyLoadedData::load(DENSE_DATA));
    UserModeDataDecorator decorator(*data, *userData);

    const double theta = 0.7;
    std::unique_ptr<UserModeRelevanceCalculator> calc(
        UserModeRelevanceCalculator::from(decorator, *userData, theta)
    );
    std::unique_ptr<LazyKernelMatrix> lazyKernelMatrix(LazyKernelMatrix::from(decorator, *calc));
    std::unique_ptr<NaiveKernelMatrix> naiveKernelMatrix(NaiveKernelMatrix::from(decorator, *calc));

    std::vector<std::vector<float>> diagonal_sets ({lazyKernelMatrix->getDiagonals(), naiveKernelMatrix->getDiagonals()});
    for (const std::vector<float> & diagonals : diagonal_sets) {
        for (const float & d : diagonals) {
            CHECK(d > 0);
        }
    }

    for (size_t j = 0; j < decorator.totalRows(); j++) {
        for (size_t i = 0; i < decorator.totalRows(); i++) {
            const float lazyScore = lazyKernelMatrix->get(j, i);
            const float naiveScore = naiveKernelMatrix->get(j, i);
            CHECK(lazyScore > 0);
            CHECK(naiveScore > 0);
            CHECK(lazyScore == naiveScore);
        }
    }
}

TEST_CASE("testing calculators with user-data") {
    LoggerHelper::setupLoggers();
    std::unique_ptr<UserData> userData(UserDataImplementation::from(
        11, 
        5, 
        std::vector<unsigned long long>({0, 4, 5}), 
        std::vector<double>({2.1245, 1.125, 1.43123})));
    std::unique_ptr<FullyLoadedData> data(FullyLoadedData::load(DENSE_DATA));
    UserModeDataDecorator decorator(*data, *userData);
    const size_t k = userData->getCu().size();
    const float epsilon = 0.01;
    const double theta = 0.7;

    auto fastRes = testCalculator(
        new FastSubsetCalculator(epsilon), 
        UserModeRelevanceCalculator::from(decorator, *userData, theta),
        decorator, 
        TranslatingUserSubset::create(decorator), 
        k, 
        epsilon);

    auto lazyFastRes = testCalculator(
        new LazyFastSubsetCalculator(epsilon), 
        UserModeRelevanceCalculator::from(decorator, *userData, theta),
        decorator, 
        TranslatingUserSubset::create(decorator), 
        k, 
        epsilon);

    checkSolutionsAreEquivalent(*fastRes, *lazyFastRes);

    CHECK(UserScore::calculateMRR(*userData, *fastRes) > 0);
    CHECK(UserScore::calculateMRR(*userData, *lazyFastRes) > 0);
}
