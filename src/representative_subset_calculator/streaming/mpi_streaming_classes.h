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
        // need space for the row's global index and the local rank's
        //  marginal gain when adding this row to it's local solution.
        //  Also, need space for the final stop tag.
        buffer(rowSize + 3, 0),
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
            CandidateSeed *nextSeed = this->extractSeedFromBuffer();
            if (status.MPI_TAG == CommunicationConstants::getContinueTag()) {
                readyForNextReceive();
            } else if (status.MPI_TAG == CommunicationConstants::getStopTag()) {
                std::cout << "listening buffer for rank " << this->rank << " has finished listening" << std::endl;
                isStillReceiving = false;
            } else {
                std::cout << "unrecognized tag of " << status.MPI_TAG << " for rank " << this->rank << std::endl;
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

    CandidateSeed* extractSeedFromBuffer() {
        auto endOfData = this->buffer.end() - 1;
        while (*endOfData != CommunicationConstants::endOfSendTag()) {
            endOfData--;
        }

        // Remove the stop tag. This will allow the next send to be received.
        *endOfData = 0;

        const size_t globalRowIndex = static_cast<size_t>(*(endOfData - 1));
        const float localMarginalGain = *(endOfData - 2);
        std::vector<float> data(this->buffer.begin(), endOfData - 2);
        this->rankSolution->addRow(globalRowIndex, localMarginalGain);
        return new CandidateSeed(globalRowIndex, this->factory.getFromBinary(move(data)), this->rank);
    }
};