#include <unordered_set>

class CandidateConsumer {
    public:
    // returns `true` when the consumer is still accepting seeds. `false` otherwise.
    virtual bool accept(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue, Timers &timers) = 0;
    virtual std::unique_ptr<Subset> getBestSolutionDestroyConsumer() = 0;
};

class NaiveCandidateConsumer : public CandidateConsumer {
    private:
    const unsigned int numberOfSenders;
    const std::unique_ptr<BucketTitrator> titrator;

    std::unordered_set<unsigned int> seenFirstElement;

    public: 
    static std::unique_ptr<NaiveCandidateConsumer> from(
        std::unique_ptr<BucketTitrator> titrator,
        const unsigned int numberOfSenders
    ) {
        return std::unique_ptr<NaiveCandidateConsumer>(
            new NaiveCandidateConsumer(move(titrator), numberOfSenders)
        );
    }

    NaiveCandidateConsumer(
        std::unique_ptr<BucketTitrator> titrator,
        const unsigned int numberOfSenders
    ) : 
        titrator(move(titrator)),
        numberOfSenders(numberOfSenders)
    {}

    std::unique_ptr<Subset> getBestSolutionDestroyConsumer() {
        return this->titrator->getBestSolutionDestroyTitrator();
    }

    bool accept(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue, Timers &timers) {
        bool stillAcceptingSeeds = true;
        if (this->seenFirstElement.size() < numberOfSenders) {
        if (!this->titrator->bucketsInitialized()) {
            timers.initBucketsTimer.startTimer();
            this->findFirstSeedsFromSenders(seedQueue);
            timers.initBucketsTimer.stopTimer();
        } 
        if (this->seenFirstElement.size() == numberOfSenders) {
            timers.insertSeedsTimer.startTimer();
            stillAcceptingSeeds = this->titrator->processQueue(seedQueue);
            timers.insertSeedsTimer.stopTimer();
        }

        return stillAcceptingSeeds;
    }

    private:
    void findFirstSeedsFromSenders(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        std::vector<std::unique_ptr<CandidateSeed>> pulledFromQueue(move(seedQueue.emptyQueueIntoVector()));

        for (size_t i = 0; i < pulledFromQueue.size(); i++) {
            std::unique_ptr<CandidateSeed>& seed(pulledFromQueue[i]);
            this->seenFirstElement.insert(seed->getOriginRank());
        }

        seedQueue.emptyVectorIntoQueue(move(pulledFromQueue));
    }
};

class StreamingCandidateConsumer : public CandidateConsumer {
    private:
    const std::unique_ptr<BucketTitrator> titrator;

    public: 
    static std::unique_ptr<StreamingCandidateConsumer> from(std::unique_ptr<BucketTitrator> titrator) {
        return std::unique_ptr<StreamingCandidateConsumer>(new StreamingCandidateConsumer(move(titrator)));
    }

    StreamingCandidateConsumer(
        std::unique_ptr<BucketTitrator> titrator
    ) : titrator(move(titrator)) {}

    std::unique_ptr<Subset> getBestSolutionDestroyConsumer() {
        return this->titrator->getBestSolutionDestroyTitrator();
    }

    bool accept(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue, Timers &timers) {
        if (seedQueue.isEmpty()) {
            return this->titrator->isFull();
        }
        timers.insertSeedsTimer.startTimer();
        this->titrator->processQueueDynamicBuckets(seedQueue);
        timers.insertSeedsTimer.stopTimer();
        return this->titrator->isFull();
    }
};