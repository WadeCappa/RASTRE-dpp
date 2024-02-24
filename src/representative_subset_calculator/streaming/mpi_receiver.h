
class SeiveReceiver : Receiver {
    private:
    struct MaybePoplulatedBuffer {
        std::vector<double> buffer;
        MPI_Request request;
        MaybePoplulatedBuffer(const size_t bufferSize) : buffer(bufferSize) {}
    };
    
    std::vector<MaybePoplulatedBuffer> buffers;
    std::vector<size_t> rankToCurrentSeed;
    size_t rank;

    MaybePoplulatedBuffer &getBuffer(const int rank) {
        return buffers[rank];
    }

    void listenForNextSeed(const unsigned int rank) {
        MaybePoplulatedBuffer &buffer(getBuffer(rank));
        MPI_Irecv(
            buffer.buffer.data(), buffer.buffer.size(), 
            MPI_DOUBLE, rank, MPI_ANY_TAG, 
            MPI_COMM_WORLD, &buffer.request
        );
    }

    public:
    SeiveReceiver(const int worldSize, const size_t rowSize, const int k) 
    : 
        buffers(worldSize, MaybePoplulatedBuffer(rowSize)),
        rank(0)
    {
        for (size_t rank = 0; rank < worldSize; rank++) {
            listenForNextSeed(rank);
        }
    }

    std::unique_ptr<CandidateSeed> receiveNextSeed(bool &stillReceiving) {
        int localDummyValue = 0;
        while (true) {
            for (; this->rank < this->rankToCurrentSeed.size(); this->rank++) {
                const size_t currentSeed = this->rankToCurrentSeed[this->rank];
                int flag;
                MPI_Status status;
                MPI_Test(&(getBuffer(this->rank).request), &flag, &status);
                if (flag == 1) {
                    this->rankToCurrentSeed[this->rank]++;
                    CandidateSeed *nextSeed = new CandidateSeed(status.MPI_TAG, getBuffer(this->rank).buffer);
                    listenForNextSeed(this->rank);
                    return std::unique_ptr<CandidateSeed>(nextSeed);
                }
            }

            this->rank = 0;
        
            // Prevents the while(true) loop from being optimized out of the compiled code
            # pragma omp atomic
            localDummyValue++;
        }
    }
};
