class BucketTitrator {
    public:
    virtual ~BucketTitrator() {}
    // Returns true when this titrator is still accepting seeds, false otherwise.
    virtual bool processQueue(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) = 0;
    virtual std::unique_ptr<Subset> getBestSolutionDestroyTitrator() = 0;
    virtual bool isFull() const = 0;

    public:
    static size_t getNumberOfBuckets(const unsigned int k, const float epsilon, bool optimisticBounds) {
        return getNumberOfBuckets(optimisticBounds ? k : k * 2, epsilon);
    }

    static size_t getNumberOfBuckets(const unsigned int k, const float epsilon) {
        
        size_t numBuckets = (int)(0.5 + [](float val, float base) {
            return log2(val) / log2(base);
        }((float)(k), (1 + epsilon))) + 1;

        return numBuckets;
    }

    static float getThresholdForBucket(size_t bucketIndex, float deltaZero, float epsilon) {
        int i = bucketIndex + std::ceil(std::log(deltaZero) / std::log(1 + epsilon));
        return std::pow(1 + epsilon, i);
    }

    static float getDeltaFromSeed(const CandidateSeed& seed, const RelevanceCalculatorFactory& calcFactory, bool knowDeltaZero) {

        // Returning 0.0 here is safe since it is expected that the current deltaZero is always 
        //  greater than or equal to 0.0. All if statements that compare deltaZero to newD0 will
        //  not pass if we return 0.0 here.
        if (knowDeltaZero) {
            return 0.0;
        }

        return std::log(
            std::sqrt(
                PerRowRelevanceCalculator::getScore(seed.getData(), calcFactory, seed.getRow())
            )
        ) * 2;
    }
};

class BucketTitratorFactory {
    public:
    virtual ~BucketTitratorFactory() {}
    virtual std::unique_ptr<BucketTitrator> createWithKnownDeltaZero(const float deltaZero) const = 0;
    virtual std::unique_ptr<BucketTitrator> createWithDynamicBuckets() const = 0;
};

class LazyInitializingBucketTitrator : public BucketTitrator {
    private:
    std::optional<std::unique_ptr<BucketTitrator>> delegate;
    std::unique_ptr<BucketTitratorFactory> factory;
    const RelevanceCalculatorFactory& calcFactory;

    static float getDeltaZero(
        SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue,
        const RelevanceCalculatorFactory& calcFactory
    ) {
        std::vector<std::unique_ptr<CandidateSeed>> pulledFromQueue(move(seedQueue.emptyQueueIntoVector()));

        float deltaZero = 0.0;
        for (size_t i = 0; i < pulledFromQueue.size(); i++) {
            std::unique_ptr<CandidateSeed>& seed(pulledFromQueue[i]);
            deltaZero = std::max(deltaZero, std::log(std::sqrt(PerRowRelevanceCalculator::getScore(seed->getData(), calcFactory, seed->getRow()))) * 2);
        }

        seedQueue.emptyVectorIntoQueue(move(pulledFromQueue));
        return deltaZero;
    }

    public:
    LazyInitializingBucketTitrator(
        std::unique_ptr<BucketTitratorFactory> factory,
        const RelevanceCalculatorFactory& calcFactory
    ) : 
        factory(move(factory)), calcFactory(calcFactory)
    {}

    bool processQueue(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        if (!this->delegate.has_value()) {
            this->delegate = factory->createWithKnownDeltaZero(getDeltaZero(seedQueue, calcFactory));
            spdlog::info("Created titrator using input queue of size {0:d}", seedQueue.size());
        } 

        return this->delegate.value()->processQueue(seedQueue);
    }

    std::unique_ptr<Subset> getBestSolutionDestroyTitrator() {
        if (!this->delegate.has_value()) {
            throw std::invalid_argument("delegate should have been initialized");
        } 

        return this->delegate.value()->getBestSolutionDestroyTitrator();
    }

    bool isFull() const {
        if (!this->delegate.has_value()) {
            return false;
        } 

        return this->delegate.value()->isFull();
    }
};

class ThreeSieveBucketTitrator : public BucketTitrator {
    private:
    const float epsilon;
    const unsigned int T; // hyperparameter -- tranfer contents to next bucket after T unsuccesful inserts
    const unsigned int k;

    unsigned int t; // counts till T
    size_t currentBucketIndex;
    const size_t totalBuckets;

    // Set to true iff you already know delta zero before creating this class
    //  and you do not want deltaZero to change
    const bool knownD0;

    float deltaZero;
    std::unique_ptr<ThresholdBucket> bucket;
    std::vector<std::unique_ptr<CandidateSeed>> seedStorage;
    const RelevanceCalculatorFactory& calcFactory;

    ThreeSieveBucketTitrator(
        const float epsilon,
        const unsigned int T,
        const unsigned int k,
        const RelevanceCalculatorFactory& calcFactory,
        const size_t totalBuckets,
        const bool knownD0,
        float deltaZero,
        std::unique_ptr<ThresholdBucket> bucket
    ) : 
        totalBuckets(totalBuckets),
        bucket(move(bucket)),
        epsilon(epsilon),
        T(T),
        t(0),
        currentBucketIndex(0),
        knownD0(knownD0),
        deltaZero(deltaZero),
        k(k),
        calcFactory(calcFactory)
    {}

    static std::unique_ptr<ThreeSieveBucketTitrator> create(
        const float epsilon, 
        const unsigned int T, 
        const unsigned int k,
        const float maybeDeltaZero,

        // If true, delta zero is known at this point. Otherwise this should be false.
        const bool deltaZeroAlreadyKnown,
        const RelevanceCalculatorFactory& calcFactory) {

        size_t totalBuckets = getNumberOfBuckets(k, epsilon, deltaZeroAlreadyKnown);
        float threshold = getThresholdForBucket(totalBuckets - 1, maybeDeltaZero, epsilon);
        
        std::unique_ptr<ThresholdBucket>bucket = std::make_unique<ThresholdBucket>(threshold, k);
        spdlog::info("number of buckets {0:d} with deltaZero of {1:f}", totalBuckets, maybeDeltaZero);
        return std::unique_ptr<ThreeSieveBucketTitrator>(
            new ThreeSieveBucketTitrator(
                epsilon, 
                T, 
                k, 
                calcFactory, 
                totalBuckets, 
                false,
                maybeDeltaZero, 
                move(bucket)
            )
        );
    }

    public:
    /**
     * Only used for standalone streaming. This method will create a titrator that *does not* know delta zero, and 
     * will dynamicaly adjust buckets using input seeds.
     */
    static std::unique_ptr<ThreeSieveBucketTitrator> createWithDynamicBuckets(
        const float epsilon, 
        const unsigned int T, 
        const unsigned int k,
        const RelevanceCalculatorFactory& calcFactory) {
        
        return ThreeSieveBucketTitrator::create(epsilon, T, k, 0.0, false, calcFactory);
    }

    static std::unique_ptr<ThreeSieveBucketTitrator> createWithKnownDeltaZero(
        const float epsilon, 
        const unsigned int T, 
        const unsigned int k,
        const float deltaZero,
        const RelevanceCalculatorFactory& calcFactory) {
        
        return ThreeSieveBucketTitrator::create(epsilon, T, k, deltaZero, true, calcFactory);
    }

    bool processQueue(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        if(this->isFull()) {
            return false;
        }
        bool stillAcceptingSeeds = true;
        std::vector<std::unique_ptr<CandidateSeed>> pulledFromQueue(move(seedQueue.emptyQueueIntoVector()));

        if (pulledFromQueue.size() == 0) {
            return true;
        }

        for (size_t seedIndex = 0; seedIndex < pulledFromQueue.size(); seedIndex++) {
            std::unique_ptr<CandidateSeed> seed(move(pulledFromQueue[seedIndex]));
            
            float newD0 = getDeltaFromSeed(*seed, calcFactory, knownD0);
        
            if (newD0 > this->deltaZero) {
                SPDLOG_DEBUG("new d0 is larger, {0:f} > {1:f}", newD0, this->deltaZero);
                float threshold = getThresholdForBucket(this->totalBuckets - 1, newD0, epsilon);
                this->bucket = std::make_unique<ThresholdBucket>(threshold, k);
                this->t = 0; 
                this->currentBucketIndex = 0;
                this->deltaZero = newD0;
                this->seedStorage.clear();
            }

            if (this->bucket->attemptInsert(seed->getRow(), seed->getData())) { 
                this->t = 0; 
                this->seedStorage.push_back(move(seed));
            } else {
                this->t += 1; 
                if (this->t >= this->T && this->currentBucketIndex < this->totalBuckets) {
                    this->t = 0;
                    this->currentBucketIndex++;
                    
                    float threshold = getThresholdForBucket(this->totalBuckets - 1 - this->currentBucketIndex, this->deltaZero, epsilon);
                    this->bucket = bucket->transferContents(threshold);
                } 
            }

            if (this->isFull()) {
                // Exit early if you can no longer accept any more seeds
                spdlog::info("Titrator can't accept any more seeds. Exiting early");
                return false;
            }
        }

        return stillAcceptingSeeds;
    }

    std::unique_ptr<Subset> getBestSolutionDestroyTitrator() {
        spdlog::info("seed storage had {0:d} seeds", this->seedStorage.size());
        return bucket->returnSolutionDestroyBucket();
    }

    bool isFull() const {
        // When we don't know d0 ahead of time, this can never be true
        if (!knownD0) {
            return false;
        }

        return bucket->isFull() || this->currentBucketIndex >= this->totalBuckets;
    }
};

class ThreeSeiveBucketTitratorFactory : public BucketTitratorFactory {
    private:
    const float epsilon;
    const unsigned int T;
    const unsigned int k;
    const RelevanceCalculatorFactory& calcFactory;

    public:
    ThreeSeiveBucketTitratorFactory(
        const float epsilon,
        const unsigned int T,
        const unsigned int k,
        const RelevanceCalculatorFactory& calcFactory
    ) : epsilon(epsilon), T(T), k(k), calcFactory(calcFactory) {}

    std::unique_ptr<BucketTitrator> createWithKnownDeltaZero(
        const float deltaZero
    ) const {
        return ThreeSieveBucketTitrator::createWithKnownDeltaZero(epsilon, T, k, deltaZero, calcFactory);
    }

    std::unique_ptr<BucketTitrator> createWithDynamicBuckets() const {
        return ThreeSieveBucketTitrator::createWithDynamicBuckets(epsilon, T, k, calcFactory);
    }
};

class SieveStreamingBucketTitrator : public BucketTitrator {
    private:
    const float epsilon;
    const unsigned int numThreads;
    const unsigned int k;
    const size_t totalBuckets;

    // Set to true iff you already know delta zero before creating this class
    //  and you do not want deltaZero to change
    const bool knownD0;

    float deltaZero;
    std::vector<std::unique_ptr<ThresholdBucket>> buckets;
    std::vector<std::unique_ptr<CandidateSeed>> seedStorage;
    const RelevanceCalculatorFactory& calcFactory;

    SieveStreamingBucketTitrator(
        const unsigned int numThreads,
        const float epsilon,
        const unsigned int k,
        const size_t totalBuckets,
        const bool knownD0,
        float deltaZero,
        std::vector<std::unique_ptr<ThresholdBucket>> buckets,
        std::vector<std::unique_ptr<CandidateSeed>> seedStorage,
        const RelevanceCalculatorFactory &calcFactory
    ) :  
        epsilon(epsilon),
        numThreads(numThreads),
        k(k),
        totalBuckets(totalBuckets),
        knownD0(knownD0),
        deltaZero(deltaZero),
        buckets(move(buckets)),
        seedStorage(move(seedStorage)),
        calcFactory(calcFactory)
    {}

    static std::unique_ptr<SieveStreamingBucketTitrator> create(
        const unsigned int numThreads,
        const float epsilon,
        const unsigned int k,
        const float maybeDeltaZero,
        
        // If true, delta zero is known at this point. Otherwise this should be false.
        const bool deltaZeroAlreadyKnown,
        const RelevanceCalculatorFactory& calcFactory) {

        size_t totalBuckets = getNumberOfBuckets(k, epsilon, deltaZeroAlreadyKnown);
        std::vector<std::unique_ptr<ThresholdBucket>> buckets;

        spdlog::info("number of buckets {0:d} with maybeDeltaZero of {1:f}", totalBuckets, maybeDeltaZero);
 
        for (int i = 0; i < totalBuckets; i++) {
            float threshold = getThresholdForBucket(i, maybeDeltaZero, epsilon);
            SPDLOG_DEBUG("generated threshold of {0:f} for index of {1:d}", threshold, i);
            buckets.push_back(std::unique_ptr<ThresholdBucket>(new ThresholdBucket(threshold, k)));
        }

        return std::unique_ptr<SieveStreamingBucketTitrator>(
            new SieveStreamingBucketTitrator(
                numThreads, 
                epsilon, 
                k, 
                totalBuckets,
                deltaZeroAlreadyKnown,
                maybeDeltaZero, 
                move(buckets), 
                std::vector<std::unique_ptr<CandidateSeed>>(), 
                calcFactory
            )
        );
    }

    public:
    /**
     * Only used for standalone streaming. This method will create a titrator that *does not* know delta zero, and 
     * will dynamicaly adjust buckets using input seeds.
     */
    static std::unique_ptr<SieveStreamingBucketTitrator> createWithDynamicBuckets(
        const unsigned int numThreads,
        const float epsilon,
        const unsigned int k,
        const RelevanceCalculatorFactory& calcFactory) {
    
        return create(numThreads, epsilon, k, 0.0, false, calcFactory);
    }

    static std::unique_ptr<SieveStreamingBucketTitrator> createWithKnownDeltaZero(
        const unsigned int numThreads,
        const float epsilon,
        const unsigned int k,
        const float deltaZero,
        const RelevanceCalculatorFactory& calcFactory) {
    
        return create(numThreads, epsilon, k, deltaZero, true, calcFactory);
    }

    bool processQueue(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        if (this->isFull()) {
            return false;
        }

        std::vector<std::unique_ptr<CandidateSeed>> pulledFromQueue(move(seedQueue.emptyQueueIntoVector()));
        if (pulledFromQueue.size() == 0) {
            // We know from the previous if statement that the titrator is not full
            return true;
        }

        float currentMaxThreshold = this->buckets[this->buckets.size() - 1]->getThreshold();
        for (size_t seedIndex = 0; seedIndex < pulledFromQueue.size(); seedIndex++) { 
            std::unique_ptr<CandidateSeed> seed = move(pulledFromQueue[seedIndex]);
            
            float newD0 = getDeltaFromSeed(*seed, calcFactory, knownD0);

            if (newD0 > this->deltaZero) {
                this->deltaZero = newD0;
                float min_threshold = this->getThresholdForBucket(0, deltaZero, epsilon);
                size_t removeBucketIndexBelow = 0;
                for (removeBucketIndexBelow = 0; removeBucketIndexBelow < this->buckets.size(); removeBucketIndexBelow++) {
                    SPDLOG_TRACE("removing bucket {0:d} with threshold of {1:f}", removeBucketIndexBelow, this->buckets[removeBucketIndexBelow]->getThreshold());
                    if (this->buckets[removeBucketIndexBelow]->getThreshold() > min_threshold)
                        break;
                }

                // remove useless buckets
                this->buckets.erase(this->buckets.begin(), this->buckets.begin() + removeBucketIndexBelow);

                //TODO: start from last bucket
                for (size_t bucket = 0; bucket < this->totalBuckets; bucket++) {
                    float threshold = getThresholdForBucket(bucket, deltaZero, epsilon);
                    if (threshold > currentMaxThreshold) {
                        SPDLOG_TRACE("Adding new buket with threshold: {0:f} since the previous max threshold was {1:f}", threshold, currentMaxThreshold);
                        this->buckets.push_back(std::unique_ptr<ThresholdBucket>(new ThresholdBucket(threshold, k)));
                        currentMaxThreshold = threshold;
                    }            
                }   
            }

            // attempt insert seed in buckets
            bool seedInserted = false;
            // #pragma omp parallel for num_threads(this->numThreads) reduction(||:seedInserted)
            for (size_t bucketIndex = 0; bucketIndex < this->buckets.size(); bucketIndex++) {
                SPDLOG_TRACE("looking at bucket {0:d} with threshold {1:f} and seed {2:d}", bucketIndex, this->buckets[bucketIndex]->getThreshold(), seed->getRow());
                seedInserted = this->buckets[bucketIndex]->attemptInsert(seed->getRow(), seed->getData()) || seedInserted;
            }

            if (seedInserted) {
                this->seedStorage.push_back(move(seed));
            } else if (this->isFull()) {
                // If all buckets are full, exit early
                spdlog::info("Titrator can't accept any more seeds. Exiting early");
                return false;
            }
        }

        return this->isFull();
    }

    std::unique_ptr<Subset> getBestSolutionDestroyTitrator() {
        float bestBucketScore = 0;
        size_t bestBucketIndex = -1;
        for (size_t i = 0; i < this->buckets.size(); i++) {
            float bucketScore = this->buckets[i]->getUtility();
            if (bucketScore > bestBucketScore) {
                bestBucketScore = bucketScore;
                bestBucketIndex = i;
            }
        }

        return this->buckets[bestBucketIndex]->returnSolutionDestroyBucket();
    }

    bool isFull() const {
        // When we don't know d0 ahead of time, this can never be true
        if (!knownD0) {
            return false;
        }

        // As long as one bucket is still accepting seeds this titrator should not return false.
        bool stillAccepting = false;
        for (size_t i = 0; i < this->buckets.size(); i++) {
            stillAccepting = buckets[i]->isFull() || stillAccepting;
        }
        
        return stillAccepting;
    }
};

class SieveStreamingBucketTitratorFactory : public BucketTitratorFactory {
    private:
    const unsigned int numThreads;
    const float epsilon;
    const unsigned int k;
    const RelevanceCalculatorFactory& calcFactory;

    public:
    SieveStreamingBucketTitratorFactory(
        const unsigned int numThreads,
        const float epsilon,
        const unsigned int k,
        const RelevanceCalculatorFactory& calcFactory
    ) : numThreads(numThreads), epsilon(epsilon), k(k), calcFactory(calcFactory) {}

    std::unique_ptr<BucketTitrator> createWithKnownDeltaZero(const float deltaZero) const {
        return SieveStreamingBucketTitrator::createWithKnownDeltaZero(numThreads, epsilon, k, deltaZero, calcFactory);
    }

    std::unique_ptr<BucketTitrator> createWithDynamicBuckets() const {
        return SieveStreamingBucketTitrator::createWithDynamicBuckets(numThreads, epsilon, k, calcFactory);
    }
};