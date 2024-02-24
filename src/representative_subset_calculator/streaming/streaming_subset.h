


class StreamingSubset : public MutableSubset {
    private:
    std::unique_ptr<MutableSubset> base;
    const LocalData &data;
    std::vector<MPI_Request> sends;

    public:
    StreamingSubset(const LocalData& data) : data(data), base(NaiveMutableSubset::makeNew()) {}

    void sendRemaining() {
        // block and send remaining
    }

    double getScore() const {
        return base->getScore();
    }

    size_t getRow(const size_t index) const {
        return base->getRow(index);
    }

    size_t size() const {
        return base->size();
    }

    nlohmann::json toJson() const {
        return base->toJson();
    }

    const size_t* begin() const {
        return base->begin();
    }

    const size_t* end() const {
        return base->end();
    }

    void addRow(const size_t row, const double marginalGain) {
        const std::vector<double>& rowToSend(this->data.getRow(row));
        sends.push_back(MPI_Request());
        MPI_Request *request = &sends.back();
        
        // Use the row index as the tag
        // do test to verify that the previous send succeeded
        MPI_Isend(rowToSend.data(), rowToSend.size(), MPI_DOUBLE, 0, data.getRemoteIndexForRow(row), MPI_COMM_WORLD, request);
        this->base->addRow(row, marginalGain);
    }
};