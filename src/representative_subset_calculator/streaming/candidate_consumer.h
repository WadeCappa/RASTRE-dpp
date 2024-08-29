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
    std::unique_ptr<RelevanceCalculatorFactory> calcFactory;

    std::unordered_set<unsigned int> seenFirstElement;
    std::unordered_set<unsigned int> firstGlobalRows;
    float bestMarginal;

    public: 
    static std::unique_ptr<NaiveCandidateConsumer> from(
        std::unique_ptr<BucketTitrator> titrator,
        const unsigned int numberOfSenders
    ) {
        return std::unique_ptr<NaiveCandidateConsumer>(
            new NaiveCandidateConsumer(
                move(titrator), 
                numberOfSenders, 
                std::unique_ptr<RelevanceCalculatorFactory>(new NaiveRelevanceCalculatorFactory())
            )
        );
    }

    NaiveCandidateConsumer(
        std::unique_ptr<BucketTitrator> titrator,
        const unsigned int numberOfSenders,
        std::unique_ptr<RelevanceCalculatorFactory> calcFactory
    ) : 
        titrator(move(titrator)),
        numberOfSenders(numberOfSenders),
        bestMarginal(0),
        calcFactory(move(calcFactory))
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
    private:
    void tryToGetFirstMarginals(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        std::vector<std::unique_ptr<CandidateSeed>> pulledFromQueue(move(seedQueue.emptyQueueIntoVector()));
        std::vector<std::pair<unsigned int, float>> rowToMarginal(pulledFromQueue.size(), std::make_pair(0,0));
        
        #pragma omp parallel for
        for (size_t i = 0; i < pulledFromQueue.size(); i++) {
            std::unique_ptr<CandidateSeed>& seed(pulledFromQueue[i]);
            
            // TODO: Only process the first seed from each sender
            if (this->firstGlobalRows.find(seed->getRow()) == this->firstGlobalRows.end()) {
                const DataRow & row(seed->getData());
                float score = std::log(std::sqrt(PerRowRelevanceCalculator::getScore(row, *calcFactory))) * 2;
                rowToMarginal[i].second = score;
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


    float getDeltaZero() {
        return bestMarginal;
    }
};

class StreamingCandidateConsumer : public CandidateConsumer {
    private:
    const unsigned int numberOfSenders;
    const std::unique_ptr<BucketTitrator> titrator;
    std::unique_ptr<RelevanceCalculatorFactory> calcFactory;

    std::unordered_set<unsigned int> seenFirstElement;
    std::unordered_set<unsigned int> firstGlobalRows;

    float bestMarginal;

    public: 
    static std::unique_ptr<StreamingCandidateConsumer> from(
        std::unique_ptr<BucketTitrator> titrator,
        const unsigned int numberOfSenders
    ) {
        return std::unique_ptr<StreamingCandidateConsumer>(
            new StreamingCandidateConsumer(
                move(titrator), 
                numberOfSenders, 
                std::unique_ptr<RelevanceCalculatorFactory>(new NaiveRelevanceCalculatorFactory())
            )
        );
    }

    StreamingCandidateConsumer(
        std::unique_ptr<BucketTitrator> titrator,
        const unsigned int numberOfSenders,
        std::unique_ptr<RelevanceCalculatorFactory> calcFactory
    ) : 
        titrator(move(titrator)),
        numberOfSenders(numberOfSenders),
        bestMarginal(-1),
        calcFactory(move(calcFactory))
    {}

    std::unique_ptr<Subset> getBestSolutionDestroyConsumer() {
        return this->titrator->getBestSolutionDestroyTitrator();
    }

    void accept(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue, Timers &timers) {
        if (!this->titrator->bucketsInitialized()) {
            timers.initBucketsTimer.startTimer();
            this->getFirstMarginalAndInitBuckets(seedQueue);
            timers.initBucketsTimer.stopTimer();
        } 
        
        if (this->titrator->bucketsInitialized()) {
            timers.insertSeedsTimer.startTimer();
           
            this->titrator->processQueueDynamicBuckets(seedQueue);

                
            timers.insertSeedsTimer.stopTimer();
        }
    }

    private:
    void getFirstMarginalAndInitBuckets(SynchronousQueue<std::unique_ptr<CandidateSeed>> &seedQueue) {
        std::vector<std::unique_ptr<CandidateSeed>> pulledFromQueue(move(seedQueue.emptyQueueIntoVector()));
        
        if (pulledFromQueue.size() == 0) {
            seedQueue.emptyVectorIntoQueue(move(pulledFromQueue));
            return;
        }
            
        for (size_t i = 0; i < pulledFromQueue.size(); i++) {

            std::unique_ptr<CandidateSeed>& seed(pulledFromQueue[0]);
            const DataRow & row(seed->getData());
            bestMarginal = std::max(bestMarginal,std::log(std::sqrt(PerRowRelevanceCalculator::getScore(row, *calcFactory))) * 2);

        }
        
        this->titrator->initBuckets(bestMarginal);
        seedQueue.emptyVectorIntoQueue(move(pulledFromQueue));
    }


    float getDeltaZero() {
        return bestMarginal;
    }
};