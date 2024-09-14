
class BucketTitrator {
    public:
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

    static float getDeltaFromSeed(const CandidateSeed& seed, const RelevanceCalculatorFactory& calcFactory) {
        float delta = std::log(std::sqrt(PerRowRelevanceCalculator::getScore(seed.getData(), calcFactory))) * 2;
        // std::cout << "found delta of " << delta << " for seed " << seed.getRow() << std::endl;
        return delta;
    }
};

class BucketTitratorFactory {
    public:
    virtual std::unique_ptr<BucketTitrator> createWithKnownDeltaZero(const float deltaZero) = 0;
};

class LazyInitializingBucketTitrator : public BucketTitrator {
    private:
    std::optional<std::unique_ptr<BucketTitrator>> delegate;
    std::unique_ptr<BucketTitratorFactory> factory;

    static float getDeltaZero(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {

    }

    public:
    bool processQueue(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        if (!this->delegate.has_value()) {
            this->delegate = factory->createWithKnownDeltaZero(getDeltaZero(seedQueue));
        } 

        this->delegate.value()->processQueue(seedQueue);
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
    std::unique_ptr<RelevanceCalculatorFactory> calcFactory;

    ThreeSieveBucketTitrator(
        const float epsilon,
        const unsigned int T,
        const unsigned int k,
        std::unique_ptr<RelevanceCalculatorFactory> calcFactory,
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
        calcFactory(move(calcFactory))
    {}

    static std::unique_ptr<ThreeSieveBucketTitrator> create(
        const float epsilon, 
        const unsigned int T, 
        const unsigned int k,
        const float maybeDeltaZero,

        // If true, delta zero is known at this point. Otherwise this should be false.
        const bool deltaZeroAlreadyKnown) {

        std::unique_ptr<RelevanceCalculatorFactory> calcFactory(new NaiveRelevanceCalculatorFactory());
        size_t totalBuckets = getNumberOfBuckets(k, epsilon, deltaZeroAlreadyKnown);
        float threshold = getThresholdForBucket(totalBuckets - 1, maybeDeltaZero, epsilon);
        
        // TODO: verify that we should create this thresholdbucket with 'k' instead of 'k * 2' here
        std::unique_ptr<ThresholdBucket>bucket = std::make_unique<ThresholdBucket>(threshold, k);
        // std::cout << "number of buckets " << totalBuckets << " with deltaZero of " << maybeDeltaZero << std::endl;
        return std::unique_ptr<ThreeSieveBucketTitrator>(
            new ThreeSieveBucketTitrator(
                epsilon, 
                T, 
                k, 
                move(calcFactory), 
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
        const unsigned int k) {
        
        return ThreeSieveBucketTitrator::create(epsilon, T, k, 0.0, false);
    }

    static std::unique_ptr<ThreeSieveBucketTitrator> createWithKnownDeltaZero(
        const float epsilon, 
        const unsigned int T, 
        const unsigned int k,
        const float deltaZero) {
        
        return ThreeSieveBucketTitrator::create(epsilon, T, k, deltaZero, true);
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
            std::unique_ptr<CandidateSeed> seed = move(pulledFromQueue[seedIndex]);
            float newD0 = getDeltaFromSeed(*seed, *calcFactory);
        
            if (!this->knownD0 && newD0 > this->deltaZero) {
                // std::cout << "new d0 is larger, " << newD0 << " > " << this->deltaZero << std::endl;
                float threshold = getThresholdForBucket(this->totalBuckets - 1, newD0, epsilon);
                this->bucket = std::make_unique<ThresholdBucket>(threshold, k);
                this->t = 0; 
                this->currentBucketIndex = 0;
                this->deltaZero = newD0;
            }

            if (this->bucket->attemptInsert(seed->getRow(), seed->getData())) { 
                this->t = 0; 
                this->seedStorage.push_back(move(seed));
            } else if (this->isFull()) {
                stillAcceptingSeeds = false;
            } else {
                this->t += 1; 
                if (this->t >= this->T && this->currentBucketIndex < this->totalBuckets) {
                    this->t = 0;
                    this->currentBucketIndex++;
                    
                    float threshold = getThresholdForBucket(this->totalBuckets - 1 - this->currentBucketIndex, this->deltaZero, epsilon);
                    this->bucket = bucket->transferContents(threshold);
                } 
            }
        }

        for (size_t i = 0; i < pulledFromQueue.size(); i++) {
        }

        return stillAcceptingSeeds;
    }

    std::unique_ptr<Subset> getBestSolutionDestroyTitrator() {
        return bucket->returnSolutionDestroyBucket();
    }

    bool isFull() const {
        return bucket->isFull() || this->currentBucketIndex >= this->totalBuckets;
    }
};

class ThreeSeiveBucketTitratorFactory : public BucketTitratorFactory {
    private:
    const float epsilon;
    const unsigned int T;
    const unsigned int k;

    public:
    ThreeSeiveBucketTitratorFactory(
        const float epsilon,
        const unsigned int T,
        const unsigned int k
    ) : epsilon(epsilon), T(T), k(k) {}

    std::unique_ptr<BucketTitrator> createWithKnownDeltaZero(const float deltaZero) {
        return ThreeSieveBucketTitrator::createWithKnownDeltaZero(epsilon, T, k, deltaZero);
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
    std::vector<ThresholdBucket> buckets;
    std::vector<std::unique_ptr<CandidateSeed>> seedStorage;
    std::unique_ptr<RelevanceCalculatorFactory> calcFactory;

    SieveStreamingBucketTitrator(
        const unsigned int numThreads,
        const float epsilon,
        const unsigned int k,
        const size_t totalBuckets,
        const bool knownD0,
        float deltaZero,
        std::vector<ThresholdBucket> buckets,
        std::vector<std::unique_ptr<CandidateSeed>> seedStorage,
        std::unique_ptr<RelevanceCalculatorFactory> calcFactory
    ) :  
        epsilon(epsilon),
        numThreads(numThreads),
        k(k),
        totalBuckets(totalBuckets),
        knownD0(knownD0),
        deltaZero(deltaZero),
        buckets(move(buckets)),
        seedStorage(move(seedStorage)),
        calcFactory(move(calcFactory))
    {}

    static std::unique_ptr<SieveStreamingBucketTitrator> create(
        const unsigned int numThreads,
        const float epsilon,
        const unsigned int k,
        const float maybeDeltaZero,
        
        // If true, delta zero is known at this point. Otherwise this should be false.
        const bool deltaZeroAlreadyKnown) {

        std::unique_ptr<RelevanceCalculatorFactory> calcFactory(new NaiveRelevanceCalculatorFactory());
        size_t totalBuckets = getNumberOfBuckets(k, epsilon, deltaZeroAlreadyKnown);
        std::vector<ThresholdBucket> buckets;
        std::cout << "number of buckets " << totalBuckets << " with maybeDeltaZero of " << maybeDeltaZero << std::endl;
 
        for (int bucket = 0; bucket < totalBuckets; bucket++) {
            float threshold = getThresholdForBucket(bucket, maybeDeltaZero, epsilon);
            buckets.push_back(ThresholdBucket(threshold, k));
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
                move(calcFactory)
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
        const unsigned int k) {
    
        return create(numThreads, epsilon, k * 2, 0.0, false);
    }

    static std::unique_ptr<SieveStreamingBucketTitrator> createWithKnownDeltaZero(
        const unsigned int numThreads,
        const float epsilon,
        const unsigned int k,
        const float deltaZero) {
    
        return create(numThreads, epsilon, k * 2, deltaZero, true);
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

        float currentMaxThreshold = this->buckets[this->buckets.size() - 1].getThreshold();
        for (size_t seedIndex = 0; seedIndex < pulledFromQueue.size(); seedIndex++) { 
            std::unique_ptr<CandidateSeed> seed = move(pulledFromQueue[seedIndex]);
            float newD0 = getDeltaFromSeed(*seed, *calcFactory);

            if (!this->knownD0 && newD0 > this->deltaZero) {
                this->deltaZero = newD0;
                float min_threshold = this->getThresholdForBucket(0, deltaZero, epsilon);
                size_t removeBucketIndexBelow = 0;
                for (removeBucketIndexBelow = 0; removeBucketIndexBelow < this->buckets.size(); removeBucketIndexBelow++) {
                    if (this->buckets[removeBucketIndexBelow].getThreshold() > min_threshold)
                        break;
                }

                // remove useless buckets
                this->buckets.erase(this->buckets.begin(), this->buckets.begin() + removeBucketIndexBelow);

                //TODO: start from last bucket
                for (size_t bucket = 0; bucket < this->totalBuckets; bucket++) {
                    float threshold = getThresholdForBucket(bucket, deltaZero, epsilon);
                    if (threshold > currentMaxThreshold) {
                        // std::cout << "Adding new buket with threshold: " << threshold << " since the previous max threshold was " << currentMaxThreshold << std::endl;
                        this->buckets.push_back(ThresholdBucket(threshold, k));
                        currentMaxThreshold = threshold;
                    }            
                }   
            }

            // attempt insert seed in buckets
            #pragma omp parallel for num_threads(this->numThreads)
            for (size_t bucketIndex = 0; bucketIndex < this->buckets.size(); bucketIndex++) {
                this->buckets[bucketIndex].attemptInsert(seed->getRow(), seed->getData());
            }

            this->seedStorage.push_back(move(seed));
        }

        return this->isFull();
    }

    std::unique_ptr<Subset> getBestSolutionDestroyTitrator() {
        float bestBucketScore = 0;
        size_t bestBucketIndex = -1;
        for (size_t i = 0; i < this->buckets.size(); i++) {
            float bucketScore = this->buckets[i].getUtility();
            if (bucketScore > bestBucketScore) {
                bestBucketScore = bucketScore;
                bestBucketIndex = i;
            }
        }

        return this->buckets[bestBucketIndex].returnSolutionDestroyBucket();
    }

    bool isFull() const {
        // As long as one bucket is still accepting seeds this titrator should not return false.
        bool stillAccepting = false;
        for (const ThresholdBucket& bucket : this->buckets) {
            stillAccepting = stillAccepting || bucket.isFull();
        }
        
        return stillAccepting;
    }
};

class SieveStreamingBucketTitratorFactory : public BucketTitratorFactory {
    private:
    const unsigned int numThreads;
    const float epsilon;
    const unsigned int k;

    public:
    SieveStreamingBucketTitratorFactory(
        const unsigned int numThreads,
        const float epsilon,
        const unsigned int k
    ) : numThreads(numThreads), epsilon(epsilon), k(k) {}

    std::unique_ptr<BucketTitrator> createWithKnownDeltaZero(const float deltaZero) {
        return SieveStreamingBucketTitrator::createWithKnownDeltaZero(numThreads, epsilon, k, deltaZero);
    }
};