

class BucketTitrator {
    public:
    virtual void processQueue(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) = 0;
    virtual std::unique_ptr<Subset> getBestSolutionDestroyTitrator() = 0;
    virtual bool bucketsInitialized() const = 0;
    virtual void initBuckets(const double deltaZero) = 0;

    protected:
    static size_t getNumberOfBuckets(const unsigned int k, const double epsilon) {
        size_t numBuckets = (int)(0.5 + [](double val, double base) {
            return log2(val) / log2(base);
        }((double)k, (1 + epsilon))) + 1;

        return numBuckets;
    }
};

class ThreeSieveBucketTitrator : public BucketTitrator {
    private:
    bool firstBucketBuilt;
    const double epsilon;
    const unsigned int T; // hyperparameter -- tranfer contents to next bucket after T unsuccesful inserts
    const unsigned int k;

    unsigned int t; // counts till T
    size_t currentBucketIndex;
    size_t totalBuckets;
    double deltaZero;
    std::unique_ptr<ThresholdBucket> bucket;
    std::vector<std::unique_ptr<CandidateSeed>> seedStorage;

    public:
    ThreeSieveBucketTitrator(
        const double epsilon,
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

    void processQueue(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        std::vector<std::unique_ptr<CandidateSeed>> pulledFromQueue(move(seedQueue.emptyQueueIntoVector()));
        int fails = 0;
        for (size_t seedIndex = 0; seedIndex < pulledFromQueue.size(); seedIndex++) {
            std::unique_ptr<CandidateSeed>& seed = pulledFromQueue[seedIndex];
            if (bucket->attemptInsert(seed->getRow(), seed->getData())) {
                this->t = 0; 
            }
            else {
                this->t += 1; 
                fails += 1;
                if (this->t >= this->T && this->currentBucketIndex < this->totalBuckets) {
                    this->t = 0;
                    this->currentBucketIndex++;

                    const double deltaZero = this->deltaZero;
                    int i = this->totalBuckets - 1 - this->currentBucketIndex + std::ceil( std::log(deltaZero) / std::log(1 + epsilon));
                    const double threshold = std::pow(1 + epsilon, i);
                    std::cout << "Bucket Threshold: " << threshold << std::endl;
                    //transfer current contents to next bucket
                    this->bucket = bucket->transferContents(threshold); //TODO:: change this to use the other constructor
                }
            }
        }

        for (size_t i = 0; i < pulledFromQueue.size(); i++) {
            this->seedStorage.push_back(move(pulledFromQueue[i]));
        }
        std::cout << "Fails: " << fails << std::endl;
    }

    std::unique_ptr<Subset> getBestSolutionDestroyTitrator() {
        return bucket->returnSolutionDestroyBucket();
    }

    bool bucketsInitialized() const {
        return this->firstBucketBuilt;
    }

    void initBuckets(const double deltaZero) {
        this->deltaZero = deltaZero;
        this->totalBuckets = this->getNumberOfBuckets(this->k, this->epsilon);

        std::cout << "number of buckets " << this->totalBuckets << " with deltaZero of " << deltaZero << std::endl;
        int i = this->totalBuckets - 1 + std::ceil( std::log(deltaZero) / std::log(1 + epsilon));
        double threshold = std::pow(1 + epsilon, i);
        std::cout << "Bucket Threshold: " << threshold << std::endl;
        this->bucket = std::make_unique<ThresholdBucket>(threshold, k);
        this->firstBucketBuilt = true;
    }

    private:
};

class SieveStreamingBucketTitrator : public BucketTitrator {
    private:
    const double epsilon;
    const unsigned int numThreads;
    const unsigned int k;

    std::vector<ThresholdBucket> buckets;
    std::vector<std::unique_ptr<CandidateSeed>> seedStorage;

    public:
    SieveStreamingBucketTitrator(
        const unsigned int numThreads,
        const double epsilon,
        const unsigned int k
    ) :  
        numThreads(numThreads),
        epsilon(epsilon),
        k(k)
    {}

    void processQueue(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        std::vector<std::unique_ptr<CandidateSeed>> pulledFromQueue(move(seedQueue.emptyQueueIntoVector()));

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
    }

    std::unique_ptr<Subset> getBestSolutionDestroyTitrator() {
        double bestBucketScore = 0;
        size_t bestBucketIndex = -1;
        for (size_t i = 0; i < this->buckets.size(); i++) {
            double bucketScore = this->buckets[i].getUtility();
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

    void initBuckets(const double deltaZero) {
        const size_t numBuckets = this->getNumberOfBuckets(this->k, this->epsilon);

        std::cout << "number of buckets " << numBuckets << " with deltaZero of " << deltaZero << std::endl;

        for (int bucket = 0; bucket < numBuckets; bucket++)
        {
            int i = bucket + std::ceil( std::log(deltaZero) / std::log(1 + epsilon));
            double threshold = (double)std::pow(1 + epsilon, i);
            this->buckets.push_back(ThresholdBucket(threshold, k));
        }
    }
};
