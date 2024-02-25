



class MpiReceiver : public Receiver {
    public:
    static std::unique_ptr<Receiver> buildReceiver(
        const unsigned int worldSize, 
        const size_t rowSize, 
        const int k
    ) {
        return std::unique_ptr<Receiver>(
            new NaiveReceiver(
                getRankBuffers(worldSize, rowSize), 
                worldSize
            )
        );
    }

    private:
    static std::vector<std::unqiue_ptr<RankBuffer>> getRankBuffers(
        const unsigned int worldSize, 
        const size_t rowSize
    ) {
        std::vector<RankBuffer> res;
        for (size_t rank = 0; rank < worldSize; rank++) {
            res.push_back(std::unique_ptr<RankBuffer>(MpiRankBuffer(rowSize, rank)));
        }

        return move(res);
    }
};