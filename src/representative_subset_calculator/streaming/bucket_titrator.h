

class BucketTitrator {
    public:
    virtual void processQueue(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) = 0;
    virtual std::unique_ptr<Subset> getBestSolutionDestroyTitrator() = 0;
    virtual bool bucketsInitialized() const = 0;
    virtual void initBuckets(const double deltaZero) = 0;

    protected:
    static unsigned int getNumberOfBuckets(const unsigned int k, const double epsilon) {
        int numBuckets = (int)(0.5 + [](double val, double base) {
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
    unsigned int currentBucketIndex;
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

        for (size_t seedIndex = 0; seedIndex < pulledFromQueue.size(); seedIndex++) {
            std::unique_ptr<CandidateSeed>& seed = pulledFromQueue[seedIndex];
            if (bucket->attemptInsert(seed->getRow(), seed->getData())) {
                this->t = 0; 
            }
            else {
                this->t += 1; 

                if (this->t >= this->T) {
                    this->t = 0;
                    this->currentBucketIndex++;

                    const double deltaZero = this->deltaZero;
                    const unsigned int numBuckets = this->getNumberOfBuckets(this->k, this->epsilon);
                    const double threshold = ((double)deltaZero / (double)( 2 * k )) * (double)std::pow(1 + epsilon, numBuckets - 1 - this->currentBucketIndex);
                    //transfer current contents to next bucket
                    this->bucket = bucket->transferContents(threshold); //TODO:: change this to use the other constructor
                }
            }
        }

        for (size_t i = 0; i < pulledFromQueue.size(); i++) {
            this->seedStorage.push_back(move(pulledFromQueue[i]));
        }
    }

    std::unique_ptr<Subset> getBestSolutionDestroyTitrator() {
        return bucket->returnSolutionDestroyBucket();
    }

    bool bucketsInitialized() const {
        return this->firstBucketBuilt;
    }

    void initBuckets(const double deltaZero) {
        this->deltaZero = deltaZero;
        const unsigned int numBuckets = this->getNumberOfBuckets(this->k, this->epsilon);

        std::cout << "number of buckets " << numBuckets << " with deltaZero of " << deltaZero << std::endl;
        // TODO:: why that extra 2 in (delta_zero / 2k). Check later
        double threshold = ((double)deltaZero / (double)( 2 * k )) * (double)std::pow(1 + epsilon, numBuckets - 1);
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
        const unsigned int numBuckets = this->getNumberOfBuckets(this->k, this->epsilon);

        std::cout << "number of buckets " << numBuckets << " with deltaZero of " << deltaZero << std::endl;

        for (int bucket = 0; bucket < numBuckets; bucket++)
        {
            double threshold = ((double)deltaZero / (double)( 2 * k )) * (double)std::pow(1 + epsilon, bucket);
            this->buckets.push_back(ThresholdBucket(threshold, k));
        }
    }
};
