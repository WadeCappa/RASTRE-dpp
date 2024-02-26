#include <atomic>

class Receiver {
    public:
    virtual std::unique_ptr<CandidateSeed> receiveNextSeed(std::atomic_bool &stillReceiving) = 0;
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
};