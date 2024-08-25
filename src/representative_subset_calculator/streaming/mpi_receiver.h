



class MpiReceiver : public Receiver {
    public:
    static std::unique_ptr<Receiver> buildReceiver(
        const unsigned int worldSize, 
        const size_t rowSize, 
        const DataRowFactory &factory
    ) {
        return std::unique_ptr<Receiver>(
            new NaiveReceiver(
                getRankBuffers(worldSize, rowSize, factory)
            )
        );
    }

    private:
    static std::vector<std::unique_ptr<RankBuffer>> getRankBuffers(
        const unsigned int worldSize, 
        const size_t rowSize,
        const DataRowFactory &factory
    ) {
        std::vector<std::unique_ptr<RankBuffer>> res;
        for (size_t rank = 1; rank < worldSize; rank++) {
            res.push_back(
                std::unique_ptr<RankBuffer>(
                    dynamic_cast<RankBuffer*>(
                        new MpiRankBuffer(rank, rowSize, factory)
                    )
                )
            );
        }

        return move(res);
    }
};

class ZeroMarginalReceiver: public Receiver {
    private:
    std::unique_ptr<Receiver> delegate;

    public:
    ZeroMarginalReceiver(std::unique_ptr<Receiver> delegate) 
    : delegate(move(delegate)) {}

    std::unique_ptr<CandidateSeed> receiveNextSeed(std::atomic_bool &stillReceiving) {
        return delegate->receiveNextSeed(stillReceiving);
    }

    std::unique_ptr<Subset> getBestReceivedSolution() {
        delegate->getBestReceivedSolution();
        return Subset::empty();
    }
};