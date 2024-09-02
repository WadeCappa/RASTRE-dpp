

class BucketTitrator {
    public:
    // Returns true when this titrator is still accepting seeds, false otherwise.
    virtual bool processQueueDynamicBuckets(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) = 0;
    virtual std::unique_ptr<Subset> getBestSolutionDestroyTitrator() = 0;
    virtual bool isFull() const = 0;

    protected:
    static size_t getNumberOfBuckets(const unsigned int k, const float epsilon) {
        size_t numBuckets = (int)(0.5 + [](float val, float base) {
            return log2(val) / log2(base);
        }((float)(2 * k), (1 + epsilon))) + 1;

        return numBuckets;
    }

    static double getThresholdForBucket(size_t bucketIndex, double deltaZero, double epsilon) {
        int i = bucketIndex + std::ceil(std::log(deltaZero) / std::log(1 + epsilon));
        return std::pow(1 + epsilon, i);
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
        float deltaZero,
        std::unique_ptr<ThresholdBucket> bucket
    ) : 
        totalBuckets(totalBuckets),
        bucket(move(bucket)),
        epsilon(epsilon),
        T(T),
        t(0),
        currentBucketIndex(0),
        deltaZero(deltaZero),
        k(k),
        calcFactory(move(calcFactory))
    {}

    public:
    static std::unique_ptr<ThreeSieveBucketTitrator> create(
        const float epsilon,
        const unsigned int T,
        const unsigned int k) {

        std::unique_ptr<RelevanceCalculatorFactory> calcFactory(new NaiveRelevanceCalculatorFactory());
        size_t totalBuckets = getNumberOfBuckets(k, epsilon);
        const float firstDeltaZero = 0.0;
        double threshold = getThresholdForBucket(totalBuckets - 1, firstDeltaZero, epsilon);
        std::unique_ptr<ThresholdBucket>bucket = std::make_unique<ThresholdBucket>(threshold, k);
        return std::unique_ptr<ThreeSieveBucketTitrator>(
            new ThreeSieveBucketTitrator(
                epsilon, 
                T, 
                k, 
                move(calcFactory), 
                totalBuckets, 
                firstDeltaZero, 
                move(bucket)
            )
        );
    }

    bool processQueueDynamicBuckets(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        if(this->bucket->isFull()) {
            return false;
        }
        bool stillAcceptingSeeds = true;
        std::vector<std::unique_ptr<CandidateSeed>> pulledFromQueue(move(seedQueue.emptyQueueIntoVector()));
        float currentMaxThreshold = this->bucket->getThreshold();

        if (pulledFromQueue.size() == 0) {
            return true;
        }

        double newD0 = 0.0;
        #pragma omp parallel for reduction(max:newD0)
        for (size_t seedIndex = 0; seedIndex < pulledFromQueue.size(); seedIndex++) {
            std::unique_ptr<CandidateSeed>& seed = pulledFromQueue[seedIndex];
            newD0 = std::log(std::sqrt(PerRowRelevanceCalculator::getScore(seed->getData(), *calcFactory))) * 2;
        }
        if (newD0 > this->deltaZero) {
            std::cout << "new d0 is larger, " << newD0 << " > " << this->deltaZero << std::endl;
            double threshold = getThresholdForBucket(this->totalBuckets - 1, deltaZero, epsilon);
            std::cout << "Adding new buket with threshold: " << threshold << " since the previous max threshold was " << currentMaxThreshold << std::endl;
            this->bucket = std::make_unique<ThresholdBucket>(threshold, k);
            this->t = 0; 
            this->currentBucketIndex = 0;
            this->deltaZero = newD0;
        }

        for (size_t seedIndex = 0; seedIndex < pulledFromQueue.size(); seedIndex++) {
            std::unique_ptr<CandidateSeed>& seed = pulledFromQueue[seedIndex];
            if (this->bucket->attemptInsert(seed->getRow(), seed->getData())) { 
                this->t = 0; 
            } else if(bucket->isFull() || this->currentBucketIndex >= this->totalBuckets) {
                stillAcceptingSeeds = false;
                std::cout << "can no longer accept seeds, full" << std::endl;
            } else {
                this->t += 1; 
                if (this->t >= this->T && this->currentBucketIndex < this->totalBuckets) {
                    this->t = 0;
                    this->currentBucketIndex++;
                    double threshold = getThresholdForBucket(this->totalBuckets - 1 - this->currentBucketIndex, deltaZero, epsilon);
                    std::cout << "Bucket Threshold: " << threshold << std::endl;
                    this->bucket = bucket->transferContents(threshold);
                } else {
                    std::cout << "cannot create new bucket" << std::endl;
                }
            }
        }

        for (size_t i = 0; i < pulledFromQueue.size(); i++) {
            this->seedStorage.push_back(move(pulledFromQueue[i]));
        }   
        return stillAcceptingSeeds; 
    }

    std::unique_ptr<Subset> getBestSolutionDestroyTitrator() {
        return bucket->returnSolutionDestroyBucket();
    }

    bool isFull() const {
        return this->bucket->isFull();
    }
};

class SieveStreamingBucketTitrator : public BucketTitrator {
    private:
    const float epsilon;
    const unsigned int numThreads;
    const unsigned int k;
    const size_t totalBuckets;
    float deltaZero;
    std::vector<ThresholdBucket> buckets;
    std::vector<std::unique_ptr<CandidateSeed>> seedStorage;
    std::unique_ptr<RelevanceCalculatorFactory> calcFactory;

    SieveStreamingBucketTitrator(
        const unsigned int numThreads,
        const float epsilon,
        const unsigned int k,
        const size_t totalBuckets,
        float deltaZero,
        std::vector<ThresholdBucket> buckets,
        std::vector<std::unique_ptr<CandidateSeed>> seedStorage,
        std::unique_ptr<RelevanceCalculatorFactory> calcFactory
    ) :  
        epsilon(epsilon),
        numThreads(numThreads),
        k(k),
        totalBuckets(totalBuckets),
        deltaZero(deltaZero),
        buckets(move(buckets)),
        seedStorage(move(seedStorage)),
        calcFactory(move(calcFactory))
    {}

    public:
    static std::unique_ptr<SieveStreamingBucketTitrator> create(
        const unsigned int numThreads,
        const float epsilon,
        const unsigned int k) {

        const float firstDeltaZero = 0.0;

        std::unique_ptr<RelevanceCalculatorFactory> calcFactory(new NaiveRelevanceCalculatorFactory());
        size_t totalBuckets = getNumberOfBuckets(k, epsilon);
        double threshold = getThresholdForBucket(totalBuckets - 1, firstDeltaZero, epsilon);
        std::unique_ptr<ThresholdBucket>bucket = std::make_unique<ThresholdBucket>(threshold, k);
        std::vector<ThresholdBucket> buckets(move(initBuckets(firstDeltaZero, epsilon, k)));
        return std::unique_ptr<SieveStreamingBucketTitrator>(
            new SieveStreamingBucketTitrator(
                numThreads, 
                epsilon, 
                k, 
                totalBuckets,
                firstDeltaZero, 
                move(buckets), 
                std::vector<std::unique_ptr<CandidateSeed>>(), 
                move(calcFactory)
            )
        );
    }

    bool processQueueDynamicBuckets(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
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
            std::unique_ptr<CandidateSeed>& seed = pulledFromQueue[seedIndex];
            this->deltaZero = std::max(this->deltaZero, std::log(std::sqrt(PerRowRelevanceCalculator::getScore(seed->getData(), *calcFactory))) * 2);
        }

        int i = std::ceil(std::log(this->deltaZero) / std::log(1 + this->epsilon));
        float min_threshold = (float)std::pow(1 + this->epsilon, i);
        i += (this->totalBuckets - 1);
        float max_threshold = (float)std::pow(1 + this->epsilon, i);
        size_t removeBucketIndexBelow = 0;
        for (removeBucketIndexBelow = 0; removeBucketIndexBelow < this->totalBuckets; removeBucketIndexBelow++) {
            if (this->buckets[removeBucketIndexBelow].getThreshold() > min_threshold)
                break;
        }

        // remove useless buckets
        this->buckets.erase(this->buckets.begin(),this->buckets.begin() + removeBucketIndexBelow);

        //TODO:: Start from last bucket
        for (size_t bucket = 0; bucket < this->totalBuckets; bucket++) {
            double threshold = getThresholdForBucket(bucket, deltaZero, epsilon);
            if (threshold > currentMaxThreshold) {
                std::cout << "Adding new buket with threshold: " << threshold << " since the previous max threshold was " << currentMaxThreshold << std::endl;
                this->buckets.push_back(ThresholdBucket(threshold, k));
                currentMaxThreshold = threshold;
            }            
        }    

        #pragma omp parallel for num_threads(this->numThreads)
        for (size_t bucketIndex = 0; bucketIndex < this->buckets.size(); bucketIndex++) {

            for (size_t seedIndex = 0; seedIndex < pulledFromQueue.size(); seedIndex++) {
                std::unique_ptr<CandidateSeed>& seed = pulledFromQueue[seedIndex];
                this->buckets[bucketIndex].attemptInsert(seed->getRow(), seed->getData());
            }            
        }   

        for (size_t i = 0; i < pulledFromQueue.size(); i++) {
            this->seedStorage.push_back(move(pulledFromQueue[i]));
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

    private:
    static std::vector<ThresholdBucket> initBuckets(const float deltaZero, const double epsilon, const unsigned int k) {
        std::vector<ThresholdBucket> res;
        const size_t numBuckets = getNumberOfBuckets(k, epsilon);
        std::cout << "number of buckets " << numBuckets << " with deltaZero of " << deltaZero << std::endl;

        for (int bucket = 0; bucket < numBuckets; bucket++) {
            double threshold = getThresholdForBucket(bucket, deltaZero, epsilon);
            res.push_back(ThresholdBucket(threshold, k));
        }

        return move(res);
    }
};
