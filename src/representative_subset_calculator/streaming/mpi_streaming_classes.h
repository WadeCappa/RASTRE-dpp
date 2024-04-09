#include <queue>
#include <future>
#include <optional>
#include <chrono>

class MpiSendRequest {
    private:
    const std::vector<double> rowToSend;
    MPI_Request request;

    public:
    MpiSendRequest(std::vector<double> rowToSend) 
    : rowToSend(move(rowToSend)) {}

    void isend(const unsigned int tag) {
        if (tag == CommunicationConstants::getStopTag()) {
            std::cout << "sending seed with tag " << tag << std::endl;
        }
        MPI_Isend(rowToSend.data(), rowToSend.size(), MPI_DOUBLE, 0, tag, MPI_COMM_WORLD, &request);
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

    std::vector<double> buffer;
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

    double getLocalSolutionScore() const {
        return this->rankSolution->getScore();
    }

    std::unique_ptr<Subset> getLocalSolutionDestroyBuffer() {
        return move(MutableSubset::upcast(move(this->rankSolution)));
    }

    private:
    void readyForNextReceive() {
        MPI_Irecv(
            buffer.data(), buffer.size(), 
            MPI_DOUBLE, rank, MPI_ANY_TAG, 
            MPI_COMM_WORLD, &request
        );
    }

    CandidateSeed* extractSeedFromBuffer() {
        size_t endOfData = this->buffer.size() - 1;
        while (this->buffer[endOfData] != CommunicationConstants::endOfSendTag()) {
            endOfData--;
        }

        endOfData--;

        const unsigned int globalRowIndex = this->buffer[endOfData--];
        const double localMarginalGain = this->buffer[endOfData];
        std::vector<double> data(this->buffer.begin(), this->buffer.begin() + endOfData);
        this->rankSolution->addRow(globalRowIndex, localMarginalGain);
        return new CandidateSeed(globalRowIndex, this->factory.getFromBinary(move(data)), this->rank);
    }
};