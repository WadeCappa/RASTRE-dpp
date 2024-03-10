#include <unordered_set>

class CandidateConsumer {
    public:
    virtual void accept(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue, Timers &timers) = 0;
    virtual std::unique_ptr<Subset> getBestSolutionDestroyConsumer() = 0;
};

class NaiveCandidateConsumer : public CandidateConsumer {
    private:
    const unsigned int numberOfSenders;
    const std::unique_ptr<BucketTitrator> titrator;

    std::unordered_set<unsigned int> seenFirstElement;
    std::unordered_set<unsigned int> firstGlobalRows;
    double bestMarginal;

    public:
    NaiveCandidateConsumer(
        std::unique_ptr<BucketTitrator> titrator,
        const unsigned int numberOfSenders
    ) : 
        titrator(move(titrator)),
        numberOfSenders(numberOfSenders),
        bestMarginal(0)
    {}

    std::unique_ptr<Subset> getBestSolutionDestroyConsumer() {
        return this->titrator->getBestSolutionDestroyTitrator();
    }

    void accept(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue, Timers &timers) {
        if (!this->titrator->bucketsInitialized()) {
            timers.initBucketsTimer.startTimer();
            this->tryToGetFirstMarginals(seedQueue);
            timers.initBucketsTimer.stopTimer();
        } 
        
        if (this->titrator->bucketsInitialized()) {
            timers.insertSeedsTimer.startTimer();
            this->titrator->processQueue(seedQueue);
            timers.insertSeedsTimer.stopTimer();
        }
    }

    private:
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
                bestMarginal = std::max(bestMarginal, p.second);
            }
        }

        for (size_t i = 0; i < pulledFromQueue.size(); i++) {
            std::unique_ptr<CandidateSeed>& seed(pulledFromQueue[i]);
            if (this->seenFirstElement.find(seed->getOriginRank()) == this->seenFirstElement.end()) {
                this->seenFirstElement.insert(seed->getOriginRank());
            }
        }

        if (this->seenFirstElement.size() == numberOfSenders) {
            this->titrator->initBuckets(this->getDeltaZero());
        }

        seedQueue.emptyVectorIntoQueue(move(pulledFromQueue));
    }

    double getDeltaZero() {
        return bestMarginal;
    }
};