

class BucketTitrator {
    public:
    // Returns true when this titrator is still accepting seeds, false otherwise.
    virtual bool processQueueDynamicBuckets(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) = 0;
    virtual std::unique_ptr<Subset> getBestSolutionDestroyTitrator() = 0;
    virtual bool bucketsInitialized() const = 0;
    virtual void initBuckets(const float deltaZero) = 0;

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
    bool firstBucketBuilt;
    const float epsilon;
    const unsigned int T; // hyperparameter -- tranfer contents to next bucket after T unsuccesful inserts
    const unsigned int k;

    unsigned int t; // counts till T
    size_t currentBucketIndex;
    size_t totalBuckets;
    float deltaZero;
    std::unique_ptr<ThresholdBucket> bucket;
    std::vector<std::unique_ptr<CandidateSeed>> seedStorage;

    public:
    ThreeSieveBucketTitrator(
        const float epsilon,
        const unsigned int T,
        const unsigned int k
    ) : 
        epsilon(epsilon),
        T(T),
        t(0),
        firstBucketBuilt(false),
        bucket(nullptr),
        currentBucketIndex(0),
        deltaZero(-1),
        k(k)
    {}

    bool processQueueDynamicBuckets(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        if(this->bucket->bucketFull()) {
            return false;
        }
        bool stillAcceptingSeeds = true;
        std::vector<std::unique_ptr<CandidateSeed>> pulledFromQueue(move(seedQueue.emptyQueueIntoVector()));
        float currentMaxThreshold = this->bucket->getThreshold();

        for (size_t seedIndex = 0; seedIndex < pulledFromQueue.size(); seedIndex++) {
            std::unique_ptr<CandidateSeed>& seed = pulledFromQueue[seedIndex];
            this->deltaZero = std::max(this->deltaZero, 2 * std::log(std::sqrt(seed->getData().dotProduct(seed->getData()) + 1)));
        }

        double threshold = getThresholdForBucket(this->totalBuckets - 1, deltaZero, epsilon);

        if (threshold > currentMaxThreshold) {
            std::cout << "Adding new buket with threshold: " << threshold << " since the previous max threshold was " << currentMaxThreshold << std::endl;
            this->bucket = std::make_unique<ThresholdBucket>(threshold, k);
            this->firstBucketBuilt = true;
            this->t = 0; 
            this->currentBucketIndex = 0;
        }

        for (size_t seedIndex = 0; seedIndex < pulledFromQueue.size(); seedIndex++) {
            std::unique_ptr<CandidateSeed>& seed = pulledFromQueue[seedIndex];
            if (this->bucket->attemptInsert(seed->getRow(), seed->getData())) { 
                this->t = 0; 
            } else if(bucket->isFull() || this->currentBucketIndex >= this->totalBuckets) {
                stillAcceptingSeeds = false;
            } else {
                this->t += 1; 
                if (this->t >= this->T && this->currentBucketIndex < this->totalBuckets) {
                    this->t = 0;
                    this->currentBucketIndex++;
                    const float deltaZero = this->deltaZero;
                    double threshold = getThresholdForBucket(this->totalBuckets - 1 - this->currentBucketIndex, deltaZero, epsilon);
                    std::cout << "Bucket Threshold: " << threshold << std::endl;
                    this->bucket = bucket->transferContents(threshold);
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

    bool bucketsInitialized() const {
        return this->firstBucketBuilt;
    }

    void initBuckets(const float deltaZero) {
        this->deltaZero = deltaZero;
        this->totalBuckets = this->getNumberOfBuckets(this->k, this->epsilon);

        std::cout << "number of buckets " << this->totalBuckets << " with deltaZero of " << deltaZero << std::endl;
        double threshold = getThresholdForBucket(this->totalBuckets - 1, deltaZero, epsilon);
        std::cout << "Bucket Threshold: " << threshold << std::endl;
        this->bucket = std::make_unique<ThresholdBucket>(threshold, k);
        this->firstBucketBuilt = true;
    }

    private:
};

class SieveStreamingBucketTitrator : public BucketTitrator {
    private:
    const float epsilon;
    const unsigned int numThreads;
    const unsigned int k;
    float deltaZero;
    std::vector<ThresholdBucket> buckets;
    std::vector<std::unique_ptr<CandidateSeed>> seedStorage;

    public:
    SieveStreamingBucketTitrator(
        const unsigned int numThreads,
        const float epsilon,
        const unsigned int k
    ) :  
        numThreads(numThreads),
        epsilon(epsilon),
        k(k),
        deltaZero(-1)
    {}

    bool processQueueDynamicBuckets(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        const size_t numBuckets = this->getNumberOfBuckets(this->k, this->epsilon);
        bool allBucketsFull = true;
        for (size_t bucket = 0; bucket < numBuckets; bucket++) {
            allBucketsFull = allBucketsFull && this->buckets[bucket].bucketFull();
        }
        if (allBucketsFull) 
            return false;

        std::vector<std::unique_ptr<CandidateSeed>> pulledFromQueue(move(seedQueue.emptyQueueIntoVector()));
        float currentMaxThreshold = this->buckets[this->buckets.size() - 1].getThreshold();
        for (size_t seedIndex = 0; seedIndex < pulledFromQueue.size(); seedIndex++) {

            std::unique_ptr<CandidateSeed>& seed = pulledFromQueue[seedIndex];
            this->deltaZero = std::max(deltaZero, 2 * std::log(std::sqrt(seed->getData().dotProduct(seed->getData()) + 1)));
        }

        int i = std::ceil(std::log(this->deltaZero) / std::log(1 + this->epsilon));
        float min_threshold = (float)std::pow(1 + this->epsilon, i);
        i += (numBuckets - 1);
        float max_threshold = (float)std::pow(1 + this->epsilon, i);
        size_t removeBucketIndexBelow = 0;
        for (removeBucketIndexBelow = 0; removeBucketIndexBelow < numBuckets; removeBucketIndexBelow++) {
            if (this->buckets[removeBucketIndexBelow].getThreshold() > min_threshold)
                break;
        }

        // remove useless buckets
        this->buckets.erase(this->buckets.begin(),this->buckets.begin() + removeBucketIndexBelow);

        //TODO:: Start from last bucket
        for (size_t bucket = 0; bucket < numBuckets; bucket++) {
            double threshold = getThresholdForBucket(bucket, deltaZero, epsilon);
            if (threshold > currentMaxThreshold) {
                std::cout << "Adding new buket with threshold: " << threshold << " since the previous max threshold was " << currentMaxThreshold << std::endl;
                this->buckets.push_back(ThresholdBucket(threshold, k));
                currentMaxThreshold = threshold;
            }            
        }    

        std::vector<bool> bucketsStillAcceptingSeeds(this->buckets.size(), false);

        #pragma omp parallel for num_threads(this->numThreads)
        for (size_t bucketIndex = 0; bucketIndex < this->buckets.size(); bucketIndex++) {

            for (size_t seedIndex = 0; seedIndex < pulledFromQueue.size(); seedIndex++) {
                std::unique_ptr<CandidateSeed>& seed = pulledFromQueue[seedIndex];
                bool accepted = this->buckets[bucketIndex].attemptInsert(seed->getRow(), seed->getData());
                bucketsStillAcceptingSeeds[bucketIndex] = accepted;
            }            
        }   

        for (size_t i = 0; i < pulledFromQueue.size(); i++) {
            this->seedStorage.push_back(move(pulledFromQueue[i]));
        }

        // As long as one bucket is still accepting seeds this titrator should not return false.
        bool stillAccepting = false;
        for (bool bucketAccepted : bucketsStillAcceptingSeeds) {
            stillAccepting = stillAccepting || bucketAccepted;
        }

        return stillAccepting;
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

    bool bucketsInitialized() const {
        return buckets.size() != 0;
    }

    void initBuckets(const float deltaZero) {
        this->deltaZero = deltaZero;

        const size_t numBuckets = this->getNumberOfBuckets(this->k, this->epsilon);
        std::cout << "number of buckets " << numBuckets << " with deltaZero of " << deltaZero << std::endl;

        for (int bucket = 0; bucket < numBuckets; bucket++) {
            double threshold = getThresholdForBucket(bucket, deltaZero, this->epsilon);
            this->buckets.push_back(ThresholdBucket(threshold, k));
        }
    }
};
