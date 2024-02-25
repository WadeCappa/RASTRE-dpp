

class MpiRankBuffer : public RankBuffer {
    private:
    std::vector<double> buffer;
    const unsigned int rank;
    MPI_Request request;

    public:
    MpiRankBuffer(
        const size_t bufferSize, 
        const unsigned int rank
    ) : 
        buffer(bufferSize),
        rank(rank) {
        this->readyForNextReceive();
    }

    CandidateSeed* askForData() {
        int flag;
        MPI_Status status;
        MPI_Test(&request, &flag, &status);
        if (flag == 1) {
            CandidateSeed *nextSeed = new CandidateSeed(status.MPI_TAG, getBuffer(this->rank).buffer);
            readyForNextReceive(this->rank);
            return nextSeed;
        } else {
            return nullptr;
        }
    }

    private:
    void readyForNextReceive() {
        MPI_Irecv(
            buffer.data(), buffer.size(), 
            MPI_DOUBLE, rank, MPI_ANY_TAG, 
            MPI_COMM_WORLD, &request
        );
    }
};