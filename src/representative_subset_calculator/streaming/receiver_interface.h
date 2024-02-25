
class Receiver {
    public:
    virtual std::unique_ptr<CandidateSeed> receiveNextSeed(bool &stillReceiving) = 0;
};

class NaiveReceiver : Receiver {
    private:
    
    std::vector<std::unique_ptr<RankBuffer>> buffers;
    size_t numberOfProcessorsStillReceiving;
    size_t listeningToRank;
    const unsigned int worldSize;

    public:
    NaiveReceiver(
        std::vector<std::unique_ptr<RankBuffer>> buffers, 
        const unsigned int worldSize
    ) : 
        buffers(move(buffers)),
        worldSize(worldSize),
        listeningToRank(0),
        numberOfProcessorsStillReceiving(worldSize)
    {}

    std::unique_ptr<CandidateSeed> receiveNextSeed(bool &stillReceiving) {
        while (true) {
            for (; this->listeningToRank < this->worldSize; this->listeningToRank++) {
                RankBuffer* buffer = this->buffers[this->listeningToRank].get();
                if (buffer->stillReceiving()) {
                    CandidateSeed* maybeSeed = buffer->askForData();
                    if (maybeSeed != nullptr) {
                        bool stillReceivingAfterLastSeed = buffer->stillReceiving();
                        if (!stillReceivingAfterLastSeed) {
                            this->numberOfProcessorsStillReceiving--;
                            stillReceiving = this->numberOfProcessorsStillReceiving != 0;
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