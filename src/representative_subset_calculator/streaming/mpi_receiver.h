
/**
 * This class is basically just a constructor for our NaiveReceiver class. We 
 * have seperated these classes so that our test suite doesn't need to implement MPI
 */
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