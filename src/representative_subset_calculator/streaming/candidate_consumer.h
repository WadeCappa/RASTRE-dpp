
class CandidateConsumer {
    public:
    virtual void accept(std::unique_ptr<CandidateSeed> seed) = 0;
    virtual std::unique_ptr<Subset> getBestSolutionDestroyConsumer() = 0;
};

class SeiveCandidateConsumer : public CandidateConsumer {
    private:
    const unsigned int worldSize;
    std::vector<std::unique_ptr<CandidateSeed>> seeds;
    std::vector<bool> seenFirstElementForRank;
    std::vector<ThresholdBucket> buckets;
    unsigned int ranksThatHaveSentFirstSeeds;
    const unsigned int k;
    const double epsilon;

    public:
    SeiveCandidateConsumer(
        unsigned int worldSize, unsigned int k, double epsilon
    ) 
    : 
        worldSize(worldSize),
        seenFirstElementForRank(std::vector<bool>(worldSize, false)),
        ranksThatHaveSentFirstSeeds(0),
        k(k),
        epsilon(epsilon)
    {}

    void accept(std::unique_ptr<CandidateSeed> seed) {
        if (!this->bucketsInitialized()) {
            if (this->seenFirstElementForRank[seed->getOriginRank()] == false) {
                this->seenFirstElementForRank[seed->getOriginRank()] = true; 
                ranksThatHaveSentFirstSeeds++;
            }

            this->seeds.push_back(move(seed));
            if (ranksThatHaveSentFirstSeeds == worldSize) {
                this->initBuckets();
            }
        } else {
            this->tryInsertSeedIntoBuckets(move(seed));
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

    void initBuckets() {
        const double deltaZero = this->getDeltaZero();
        const unsigned int numBuckets = this->getNumberOfBuckets(this->k, this->epsilon);

        std::cout << "number of buckets " << numBuckets << " with deltaZero of " << deltaZero << std::endl;

        for (int bucket = 0; bucket < numBuckets; bucket++)
        {
            double threshold = ((double)deltaZero / (double)( 2 * k )) * (double)std::pow(1 + epsilon, bucket);
            this->buckets.push_back(ThresholdBucket(threshold, k));
        }

        for (size_t bucketIndex = 0; bucketIndex < this->buckets.size(); bucketIndex++) {
            for (size_t seedIndex = 0; seedIndex < seeds.size(); seedIndex++) {
                const auto & seed = this->seeds[seedIndex];
                this->buckets[bucketIndex].attemptInsert(seed->getRow(), seed->getData());
            }
        }
    }

    unsigned int getNumberOfBuckets(const unsigned int k, const double epsilon) {
        int numBuckets = (int)(0.5 + [](double val, double base) {
            return log2(val) / log2(base);
        }((double)k, (1+this->epsilon))) + 1;

        return numBuckets;
    }

    void tryInsertSeedIntoBuckets(std::unique_ptr<CandidateSeed> seed) {
        for (size_t bucketIndex = 0; bucketIndex < this->buckets.size(); bucketIndex++) {
            this->buckets[bucketIndex].attemptInsert(seed->getRow(), seed->getData());
        }

        seeds.push_back(move(seed));
    }

    double getDeltaZero() {
        std::vector<double> marginals(this->seeds.size(), 0);
        #pragma omp parallel for
        for (size_t seed = 0; seed < this->seeds.size(); seed++) {
            SimilarityMatrix tempMatrix;
            tempMatrix.addRow(this->seeds[seed]->getData());
            double marginal = tempMatrix.getCoverage();
            marginals[seed] = marginal;
        }

        double highestMarginal = 0;
        for (const double m : marginals) {
            highestMarginal = std::max(highestMarginal, m);
        }

        return highestMarginal;
    }
};