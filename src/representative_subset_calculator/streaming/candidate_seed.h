


class CandidateSeed {
    private:
    std::unique_ptr<DataRow> data;
    unsigned int globalRow;
    unsigned int originRank;

    public:
    CandidateSeed(
        const unsigned int row, 
        std::unique_ptr<DataRow> data,
        const unsigned int rank
    ) : 
        data(move(data)), 
        globalRow(row),
        originRank(rank)
    {}

    const DataRow &getData() {
        return *(this->data.get());
    }

    unsigned int getRow() {
        return this->globalRow;
    }

    unsigned int getOriginRank() {
        return this->originRank;
    }
};
