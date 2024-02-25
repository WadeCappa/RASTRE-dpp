#include <queue>
#include <future>
#include <optional>
#include <chrono>

class MpiSendRequest : public SendRequest {
    private:
    const std::vector<double> rowToSend;

    std::optional<std::future<bool>> sendRequest;

    public:
    MpiSendRequest(std::vector<double> rowToSend) 
    : rowToSend(move(rowToSend)), sendRequest(std::nullopt) {}

    bool sendCompleted() const {
        if (this->sendStarted()) {
            std::future_status status = sendRequest.value().wait_for(std::chrono::seconds(0));
            return status == std::future_status::ready;
        }
        return false;
    }

    bool sendStarted() const {
        return this->sendRequest.has_value();
    }

    void sendAsync(const unsigned int tag) {
        if (sendStarted()) {
            std::cout << "attempted to send while a send was already in progress" << std::endl;
            return;
        }
        this->sendRequest = std::async(std::launch::async, sendAndBlockPrivate, rowToSend, tag); 
    }

    void sendAndBlock(const unsigned int tag) {
        sendAndBlockPrivate(this->rowToSend, tag);
    }

    bool waitForCompletion() {
        if (!sendStarted()) {
            std::cout << "attempted to wait for a send to end when no send has started" << std::endl;
            return false;
        }
        return this->sendRequest.value().get();
    }

    private:
    static bool sendAndBlockPrivate(const std::vector<double> &rowToSend, const unsigned int tag) {
        if (tag == CommunicationConstants::getStopTag()) {
            std::cout << "sending seed with tag " << tag << std::endl;
        }
        MPI_Send(rowToSend.data(), rowToSend.size(), MPI_DOUBLE, 0, tag, MPI_COMM_WORLD);
        return true;
    }
};

class MpiRankBuffer : public RankBuffer {
    private:
    const unsigned int rank;

    std::vector<double> buffer;
    MPI_Request request;
    bool isStillReceiving;

    public:
    MpiRankBuffer(
        const unsigned int rank,
        const size_t rowSize
    ) : 
        buffer(rowSize + 1),
        rank(rank),
        isStillReceiving(true)
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

    private:
    void readyForNextReceive() {
        MPI_Irecv(
            buffer.data(), buffer.size(), 
            MPI_DOUBLE, rank, MPI_ANY_TAG, 
            MPI_COMM_WORLD, &request
        );
    }

    CandidateSeed* extractSeedFromBuffer() {
        const unsigned int globalRowIndex = this->buffer.back();
        std::vector<double> data(this->buffer.begin(), this->buffer.end() - 1);
        return new CandidateSeed(globalRowIndex, move(data), this->rank);
    }
};