
#include <unordered_set>

#include "candidate_seed.h"
#include "synchronous_queue.h"
#include "../representative_subset.h"
#include "../timers/timers.h"
#include "bucket_titrator.h"

#ifndef CANDIDATE_CONSUMER_H
#define CANDIDATE_CONSUMER_H

class CandidateConsumer {
    public:
    virtual ~CandidateConsumer() {}

    // returns `true` when the consumer is still accepting seeds. `false` otherwise.
    virtual bool accept(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue, Timers &timers) = 0;
    virtual std::unique_ptr<Subset> getBestSolutionDestroyConsumer() = 0;
};

class FakeCandidateConsumer : public CandidateConsumer {
    public:
    bool accept(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue, Timers &timers) {
        seedQueue.emptyQueueIntoVector();
        return true;
    }

    std::unique_ptr<Subset> getBestSolutionDestroyConsumer() {
        return Subset::empty();
    }
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
            new NaiveCandidateConsumer(std::move(titrator), numberOfSenders)
        );
    }

    NaiveCandidateConsumer(
        std::unique_ptr<BucketTitrator> titrator,
        const unsigned int numberOfSenders
    ) : 
        titrator(std::move(titrator)),
        numberOfSenders(numberOfSenders)
    {}

    std::unique_ptr<Subset> getBestSolutionDestroyConsumer() {
        return this->titrator->getBestSolutionDestroyTitrator();
    }

    bool accept(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue, Timers &timers) {
        bool stillAcceptingSeeds = true;
        if (this->seenFirstElement.size() < numberOfSenders) {
            timers.initBucketsTimer.startTimer();
            this->findFirstSeedsFromSenders(seedQueue);
            timers.initBucketsTimer.stopTimer();
        } 
        if (this->seenFirstElement.size() >= numberOfSenders) {
            timers.insertSeedsTimer.startTimer();
            stillAcceptingSeeds = this->titrator->processQueue(seedQueue);
            timers.insertSeedsTimer.stopTimer();
        }

        return stillAcceptingSeeds;
    }

    private:
    void findFirstSeedsFromSenders(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        std::vector<std::unique_ptr<CandidateSeed>> pulledFromQueue(std::move(seedQueue.emptyQueueIntoVector()));

        for (size_t i = 0; i < pulledFromQueue.size(); i++) {
            std::unique_ptr<CandidateSeed>& seed(pulledFromQueue[i]);
            this->seenFirstElement.insert(seed->getOriginRank());
        }

        seedQueue.emptyVectorIntoQueue(std::move(pulledFromQueue));
    }
};

#endif