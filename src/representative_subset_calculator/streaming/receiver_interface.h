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
                            std::cout << "another buffer of rank " << buffer->getRank() << " has stopped receiving, only " << numberOfProcessorsStillReceiving << " buffers left" << std::endl;
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
        double bestSolution = 0;
        size_t bestRank = -1;
        for (size_t i = 0; i < this->buffers.size(); i++) {
            const double rankScore = this->buffers[i]->getLocalSolutionScore();
            std::cout << "rank " << i << " had score of " << rankScore << std::endl;
            if (rankScore > bestSolution) {
                bestRank = i;
            }
        }

        return move(this->buffers[bestRank]->getLocalSolutionDestroyBuffer());
    }
};