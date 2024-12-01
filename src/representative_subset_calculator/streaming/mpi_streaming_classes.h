#include <queue>
#include <future>
#include <optional>
#include <chrono>

class MpiSendRequest {
    private:
    const std::vector<float> rowToSend;
    MPI_Request request;

    public:
    MpiSendRequest(std::vector<float> rowToSend) 
    : rowToSend(move(rowToSend)) {}

    void isend(const unsigned int tag) {
        MPI_Isend(rowToSend.data(), rowToSend.size(), MPI_FLOAT, 0, tag, MPI_COMM_WORLD, &request);
    }

    void waitForISend() {
        MPI_Status status;
        MPI_Wait(&request, &status);
    }
};

class MpiRankBuffer : public RankBuffer {
    private:
    const unsigned int rank;
    const DataRowFactory &factory;

    std::vector<float> buffer;
    MPI_Request request;
    bool isStillReceiving;
    std::unique_ptr<MutableSubset> rankSolution;

    public:
    MpiRankBuffer(
        const unsigned int rank,
        const size_t rowSize,
        const DataRowFactory &factory
    ) : 
        // The buffer needs to hold an adjacency list of max size column size. this 
        //  means thata the buffer needs to have at least rowSize * 2 spots to 
        //  prevent an MPI_TRUNCATED_MESSAGE error. Additionally, we need 3 
        //  more spots to contain metadata for this row.
        buffer(rowSize * 2 + 3, 0),
        rank(rank),
        isStillReceiving(true),
        rankSolution(std::unique_ptr<MutableSubset>(NaiveMutableSubset::makeNew())),
        factory(factory)
    {
        this->readyForNextReceive();
    }

    CandidateSeed* askForData() {
        int flag;
        MPI_Status status;
        MPI_Test(&request, &flag, &status);
        if (flag == 1) {
            CandidateSeed *nextSeed = nullptr;
            if (status.MPI_TAG == CommunicationConstants::getContinueTag()) {
                nextSeed = this->extractSeedFromBuffer();
                this->readyForNextReceive();
            } else if (status.MPI_TAG == CommunicationConstants::getStopTag()) {
                this->extractSubsetFromBuffer();
                spdlog::info("listening buffer for rank {0:d} has finished listening", this->rank);
                isStillReceiving = false;
            } else {
                spdlog::error("unrecognized tag of {0:d} for rank {1:d}", status.MPI_TAG, this->rank);
            }
            return nextSeed;
        } else {
            return nullptr;
        }
    }

    bool stillReceiving() {
        return isStillReceiving;
    }

    unsigned int getRank() const {
        return this->rank;
    }

    float getLocalSolutionScore() const {
        return this->rankSolution->getScore();
    }

    std::unique_ptr<Subset> getLocalSolutionDestroyBuffer() {
        return move(MutableSubset::upcast(move(this->rankSolution)));
    }

    private:
    void readyForNextReceive() {
        MPI_Irecv(
            buffer.data(), buffer.size(), 
            MPI_FLOAT, rank, MPI_ANY_TAG, 
            MPI_COMM_WORLD, &request
        );
    }

    void extractSubsetFromBuffer() {
        auto endOfData = this->buffer.end();
        while (*endOfData != CommunicationConstants::endOfSendTag() && endOfData != this->buffer.begin()) {
            endOfData--;
        }

        if (endOfData == this->buffer.begin()) {
            spdlog::error("Received empty send buffer");
        }

        // Remove the stop tag. This will allow the next send to be received.
        *endOfData = 0;

        // TODO: This byte cast might be wrong now that we're sending floats. You need to double check this.
        const double marginal = *(endOfData - 1);
        std::vector<size_t> seeds(this->buffer.begin(), endOfData - 1);
        this->rankSolution = std::unique_ptr<MutableSubset>(new NaiveMutableSubset(move(seeds), marginal));
    }

    CandidateSeed* extractSeedFromBuffer() {
        auto endOfData = this->buffer.end();
        while (*endOfData != CommunicationConstants::endOfSendTag() && endOfData != this->buffer.begin()) {
            endOfData--;
        }

        if (endOfData == this->buffer.begin()) {
            spdlog::error("Received empty send buffer");
        }

        // Remove the stop tag. This will allow the next send to be received.
        *endOfData = 0;

        // TODO: This byte cast might be wrong now that we're sending floats. You need to double check this.
        const size_t globalRowIndex = static_cast<size_t>(*(endOfData - 1));
        std::vector<float> data(this->buffer.begin(), endOfData - 1);
        return new CandidateSeed(globalRowIndex, this->factory.getFromBinary(move(data)), this->rank);
    }
};
