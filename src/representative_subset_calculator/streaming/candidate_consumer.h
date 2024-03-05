#include <unordered_set>

class CandidateConsumer {
    public:
    virtual void accept(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue, Timers &timers) = 0;
    virtual std::unique_ptr<Subset> getBestSolutionDestroyConsumer() = 0;
};

class SeiveCandidateConsumer : public CandidateConsumer {
    private:
    const unsigned int numberOfSenders;
    const unsigned int k;
    const double epsilon;
    const unsigned int threads;

    std::vector<std::unique_ptr<CandidateSeed>> seedStorage;
    std::unordered_set<unsigned int> seenFirstElement;
    std::vector<ThresholdBucket> buckets;
    std::vector<double> firstMarginals;
    std::unordered_set<unsigned int> firstGlobalRows;

    public:
    SeiveCandidateConsumer(
        const unsigned int numberOfSenders, 
        const unsigned int k, 
        const double epsilon,
        const unsigned int numberOfConsumerThreads
    ) : 
        numberOfSenders(numberOfSenders),
        k(k),
        epsilon(epsilon),
        threads(numberOfConsumerThreads)
    {}

    SeiveCandidateConsumer(
        const unsigned int numberOfSenders, 
        const unsigned int k, 
        const double epsilon
    ) : 
        numberOfSenders(numberOfSenders),
        k(k),
        epsilon(epsilon),
        threads(omp_get_max_threads() - 1)
    {}

    void accept(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue, Timers &timers) {
        if (!this->bucketsInitialized()) {
            timers.initBucketsTimer.startTimer();
            this->tryToGetFirstMarginals(seedQueue);
            timers.initBucketsTimer.stopTimer();
        } 
        
        if (this->bucketsInitialized()) {
            timers.insertSeedsTimer.startTimer();
            this->processQueue(seedQueue);
            timers.insertSeedsTimer.stopTimer();
        }
    }

    std::unique_ptr<Subset> getBestSolutionDestroyConsumer() {
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

    private:
    bool bucketsInitialized() const {
        return buckets.size() != 0;
    }

    void tryToGetFirstMarginals(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        std::vector<std::unique_ptr<CandidateSeed>> pulledFromQueue(move(seedQueue.emptyQueueIntoVector()));
        std::vector<std::pair<unsigned int, double>> rowToMarginal(pulledFromQueue.size(), std::make_pair(0,0));
        
        #pragma omp parallel for
        for (size_t i = 0; i < pulledFromQueue.size(); i++) {
            std::unique_ptr<CandidateSeed>& seed(pulledFromQueue[i]);
            if (this->firstGlobalRows.find(seed->getRow()) == this->firstGlobalRows.end()) {
                MutableSimilarityMatrix tempMatrix;
                tempMatrix.addRow(seed->getData());
                rowToMarginal[i].second = tempMatrix.getCoverage();
            }

            rowToMarginal[i].first = seed->getRow();
        }

        for (const auto & p : rowToMarginal) {
            if (firstGlobalRows.find(p.first) == firstGlobalRows.end()) {
                firstGlobalRows.insert(p.first);
                firstMarginals.push_back(p.second);
            }
        }

        for (size_t i = 0; i < pulledFromQueue.size(); i++) {
            std::unique_ptr<CandidateSeed>& seed(pulledFromQueue[i]);
            if (this->seenFirstElement.find(seed->getOriginRank()) == this->seenFirstElement.end()) {
                this->seenFirstElement.insert(seed->getOriginRank());
            }
        }

        if (this->seenFirstElement.size() == numberOfSenders) {
            this->initBuckets(seedQueue);
        }

        seedQueue.emptyVectorIntoQueue(move(pulledFromQueue));
    }

    void initBuckets(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        const double deltaZero = this->getDeltaZero();
        const unsigned int numBuckets = this->getNumberOfBuckets(this->k, this->epsilon);

        std::cout << "number of buckets " << numBuckets << " with deltaZero of " << deltaZero << std::endl;

        for (int bucket = 0; bucket < numBuckets; bucket++)
        {
            double threshold = ((double)deltaZero / (double)( 2 * k )) * (double)std::pow(1 + epsilon, bucket);
            this->buckets.push_back(ThresholdBucket(threshold, k));
        }
    }

    void processQueue(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        std::vector<std::unique_ptr<CandidateSeed>> pulledFromQueue(move(seedQueue.emptyQueueIntoVector()));

        #pragma omp parallel for num_threads(this->threads)
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

    unsigned int getNumberOfBuckets(const unsigned int k, const double epsilon) {
        int numBuckets = (int)(0.5 + [](double val, double base) {
            return log2(val) / log2(base);
        }((double)k, (1+this->epsilon))) + 1;

        return numBuckets;
    }

    double getDeltaZero() {
        double highestMarginal = 0;
        for (const double m : this->firstMarginals) {
            highestMarginal = std::max(highestMarginal, m);
        }

        return highestMarginal;
    }
};


class ThreeSeiveCandidateConsumer : public CandidateConsumer {
    private:
    const unsigned int numberOfSenders;
    const unsigned int k;
    const double epsilon;
    unsigned int T; // hyperparameter -- tranfer contents to next bucket after T unsuccesful inserts
    unsigned int t; // counts till T
    unsigned int currentBucketIndex;

    std::vector<std::unique_ptr<CandidateSeed>> seedStorage;
    std::unordered_set<unsigned int> seenFirstElement;
    std::vector<ThresholdBucket> buckets;
    std::vector<double> firstMarginals;
    std::unordered_set<unsigned int> firstGlobalRows;

    public:
    ThreeSeiveCandidateConsumer(
        const unsigned int numberOfSenders, 
        const unsigned int k, 
        const double epsilon,
        unsigned int T
    ) : 
        numberOfSenders(numberOfSenders),
        k(k),
        epsilon(epsilon),
        T(T),
        t(0),
        currentBucketIndex(0)
    {}

    void accept(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue, Timers &timers) {
        if (!this->bucketsInitialized()) {
            timers.initBucketsTimer.startTimer();
            this->tryToGetFirstMarginals(seedQueue);
            timers.initBucketsTimer.stopTimer();
        } 
        
        if (this->bucketsInitialized()) {
            timers.insertSeedsTimer.startTimer();
            this->processQueue(seedQueue);
            timers.insertSeedsTimer.stopTimer();
        }
    }

    std::unique_ptr<Subset> getBestSolutionDestroyConsumer() {

        return this->buckets[this->currentBucketIndex].returnSolutionDestroyBucket();
    }

    private:
    bool bucketsInitialized() const {
        return buckets.size() != 0;
    }

    unsigned int getNumberOfBuckets(const unsigned int k, const double epsilon) {
        int numBuckets = (int)(0.5 + [](double val, double base) {
            return log2(val) / log2(base);
        }((double)k, (1+this->epsilon))) + 1;

        return numBuckets;
    }

    double getDeltaZero() {
        double highestMarginal = 0;
        for (const double m : this->firstMarginals) {
            highestMarginal = std::max(highestMarginal, m);
        }

        return highestMarginal;
    }

    void initBuckets(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        const double deltaZero = this->getDeltaZero();
        const unsigned int numBuckets = this->getNumberOfBuckets(this->k, this->epsilon);

        std::cout << "number of buckets " << numBuckets << " with deltaZero of " << deltaZero << std::endl;
        // TODO:: why that extra 2 in (delta_zero / 2k). Check later
        double threshold = ((double)deltaZero / (double)( 2 * k )) * (double)std::pow(1 + epsilon, numBuckets - 1);
        this->buckets.push_back(ThresholdBucket(threshold, k));
    }

    void tryToGetFirstMarginals(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        std::vector<std::unique_ptr<CandidateSeed>> pulledFromQueue(move(seedQueue.emptyQueueIntoVector()));
        std::vector<std::pair<unsigned int, double>> rowToMarginal(pulledFromQueue.size(), std::make_pair(0,0));
        
        #pragma omp parallel for
        for (size_t i = 0; i < pulledFromQueue.size(); i++) {
            std::unique_ptr<CandidateSeed>& seed(pulledFromQueue[i]);
            if (this->firstGlobalRows.find(seed->getRow()) == this->firstGlobalRows.end()) {
                MutableSimilarityMatrix tempMatrix;
                tempMatrix.addRow(seed->getData());
                rowToMarginal[i].second = tempMatrix.getCoverage();
            }

            rowToMarginal[i].first = seed->getRow();
        }

        for (const auto & p : rowToMarginal) {
            if (firstGlobalRows.find(p.first) == firstGlobalRows.end()) {
                firstGlobalRows.insert(p.first);
                firstMarginals.push_back(p.second);
            }
        }

        for (size_t i = 0; i < pulledFromQueue.size(); i++) {
            std::unique_ptr<CandidateSeed>& seed(pulledFromQueue[i]);
            if (this->seenFirstElement.find(seed->getOriginRank()) == this->seenFirstElement.end()) {
                this->seenFirstElement.insert(seed->getOriginRank());
            }
        }

        if (this->seenFirstElement.size() == numberOfSenders) {
            this->initBuckets(seedQueue);
        }

        seedQueue.emptyVectorIntoQueue(move(pulledFromQueue));
    }

    void processQueue(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        std::vector<std::unique_ptr<CandidateSeed>> pulledFromQueue(move(seedQueue.emptyQueueIntoVector()));

        for (size_t seedIndex = 0; seedIndex < pulledFromQueue.size(); seedIndex++) {
            std::unique_ptr<CandidateSeed>& seed = pulledFromQueue[seedIndex];
            if (this->buckets[this->currentBucketIndex].attemptInsert(seed->getRow(), seed->getData())) {
                this->t = 0; 
            }
            else {
                this->t += 1; 

                if (this->t >= this->T) {
                    this->t = 0;
                    this->currentBucketIndex++;

                    const double deltaZero = this->getDeltaZero();
                    const unsigned int numBuckets = this->getNumberOfBuckets(this->k, this->epsilon);
                    double threshold = ((double)deltaZero / (double)( 2 * k )) * (double)std::pow(1 + epsilon, numBuckets - 1 - this->currentBucketIndex);
                    //transfer current contents to next bucket
                    this->buckets.push_back(ThresholdBucket(threshold, k)); //TODO:: change this to use the other constructor
                }
            }
        }

        for (size_t i = 0; i < pulledFromQueue.size(); i++) {
            this->seedStorage.push_back(move(pulledFromQueue[i]));
        }
    }

    

};