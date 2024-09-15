#include <atomic>

class Receiver {
    public:
    virtual std::unique_ptr<CandidateSeed> receiveNextSeed(std::atomic_bool &stillReceiving) = 0;
    virtual std::unique_ptr<Subset> getBestReceivedSolution() = 0;
};

class NaiveReceiver : public Receiver {
    private:
    
    std::vector<std::unique_ptr<RankBuffer>> buffers;
    size_t numberOfProcessorsStillReceiving;
    size_t listeningToRank;

    public:
    NaiveReceiver(
        std::vector<std::unique_ptr<RankBuffer>> buffers
    ) : 
        buffers(move(buffers)),
        listeningToRank(0),
        numberOfProcessorsStillReceiving(this->buffers.size())
    {}

    std::unique_ptr<CandidateSeed> receiveNextSeed(std::atomic_bool &stillReceiving) {
        while (true) {
            for (; this->listeningToRank < this->buffers.size(); this->listeningToRank++) {
                RankBuffer* buffer = this->buffers[this->listeningToRank].get();
                if (buffer->stillReceiving()) {
                    CandidateSeed* maybeSeed = buffer->askForData();
                    if (maybeSeed != nullptr) {
                        bool stillReceivingAfterLastSeed = buffer->stillReceiving();
                        if (!stillReceivingAfterLastSeed) {
                            this->numberOfProcessorsStillReceiving--;
                            spdlog::info("another buffer of rank {0:d} has stopped receiving, only {1:d} buffers left", buffer->getRank(), numberOfProcessorsStillReceiving);
                            stillReceiving.store(this->numberOfProcessorsStillReceiving != 0);
                        }
                        this->listeningToRank++;
                        return std::unique_ptr<CandidateSeed>(maybeSeed);
                    }
                }
            }

            this->listeningToRank = 0;
        }
    }

    std::unique_ptr<Subset> getBestReceivedSolution() {
        float bestSolution = 0;
        size_t bestRank = -1;
        for (size_t i = 0; i < this->buffers.size(); i++) {
            const float rankScore = this->buffers[i]->getLocalSolutionScore();
            spdlog::info("rank {0:d} had score of {1:f}", i, rankScore);
            if (rankScore > bestSolution) {
                bestRank = i;
                bestSolution = rankScore;
            }
        }

        return move(this->buffers[bestRank]->getLocalSolutionDestroyBuffer());
    }
};