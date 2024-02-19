#include "bucket.h"
#include <mpi.h>
#include <queue>

static const int DOUBLES_FOR_ROW_INDEX = 1;

class StreamingReceiver {
    public:
    virtual std::unique_ptr<Subset> stream() = 0;
};

class SeiveStreamingReceiver : public StreamingReceiver {
    private:
    class CandidateSeed {
        private:
        std::vector<double> data;
        unsigned int globalRow;

        public:
        CandidateSeed(const unsigned int row, const std::vector<double> &data) : data(data), globalRow(row) {}

        const std::vector<double> &getData() {
            return this->data;
        }

        unsigned int getRow() {
            return this->globalRow;
        }
    };

    unsigned int bucketsInitialized;

    struct ReceiveBuffers {
        private:
        struct MaybePoplulatedBuffer {
            std::vector<double> buffer;
            MPI_Request request;
            MaybePoplulatedBuffer(const size_t bufferSize) : buffer(bufferSize) {}
        };
        
        std::vector<MaybePoplulatedBuffer> buffers;
        std::vector<size_t> rankToCurrentSeed;
        std::vector<bool> seenFirstElementFromRank;

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

        bool shouldInitializeBuckets() {
            bool seenAllElements = true;
            for (const bool seen : seenFirstElementFromRank) {
                seenAllElements = seenAllElements && seen;
            }

            return seenAllElements;
        }

        public:
        ReceiveBuffers(const int worldSize, const size_t rowSize, const int k) 
        : 
            buffers(worldSize, MaybePoplulatedBuffer(rowSize + DOUBLES_FOR_ROW_INDEX)),
            rankToCurrentSeed(worldSize, 0),
            seenFirstElementFromRank(worldSize, false)
        {
            for (size_t rank = 0; rank < worldSize; rank++) {
                listenForNextSeed(rank);
            }
        }

        CandidateSeed* receiveNextSeed(bool &bucketsInitializedFlag) {
            int localDummyValue = 0;
            while (true) {
                for (size_t rank = 0; rank < this->rankToCurrentSeed.size(); rank++) {
                    const size_t currentSeed = this->rankToCurrentSeed[rank];
                    int flag;
                    MPI_Status status;
                    MPI_Test(&(getBuffer(rank).request), &flag, &status);
                    if (flag == 1) {
                        this->rankToCurrentSeed[rank]++;
                        CandidateSeed *nextSeed = new CandidateSeed(status.MPI_TAG, getBuffer(rank).buffer);
                        listenForNextSeed(rank);
                        seenFirstElementFromRank[rank] = true;
                        
                        // By side effect, determine if buckets should be initailized 
                        if (bucketsInitializedFlag == false) {
                            bucketsInitializedFlag = shouldInitializeBuckets();
                        }

                        return nextSeed;
                    }
                }
            
                // Prevents the while(true) loop from being optimized out of the compiled code
                # pragma omp atomic
                localDummyValue++;
            }
        }
    };

    ReceiveBuffers buffers;
    std::vector<ThresholdBucket> buckets;
    std::vector<CandidateSeed*> availableBuffers;
    size_t firstUnavailableBuffer;
    const int worldSize;
    const int k;

    public:
    SeiveStreamingReceiver(const int worldSize, const size_t rowSize, const int k) 
    : 
        buffers(worldSize, rowSize + DOUBLES_FOR_ROW_INDEX, k),
        availableBuffers(worldSize * k, nullptr),
        firstUnavailableBuffer(0),
        worldSize(worldSize),
        k(k),
        bucketsInitialized(0)
    {}
    
    std::unique_ptr<Subset> stream(const int threads) {
        #pragma omp parallel num_threads(threads)
        {
            const int threadId = omp_get_thread_num();
            if (threadId == 0) {
                listenForSeeds(this->worldSize, this->k);
            } else {
                // Subtract the listening threads
                processSeeds(threads - 1);
            }
        }
    }

    private:
    void listenForSeeds(const int worldSize, const int k) {
        bool shouldInitializeBuckets = false;
        for (int receivedSeeds = 0; receivedSeeds < k * worldSize; receivedSeeds++) {
            availableBuffers[this->firstUnavailableBuffer] = buffers.receiveNextSeed(shouldInitializeBuckets);
            this->firstUnavailableBuffer++;
        }
    }

    void processSeeds(const int threadsForProcessing) {
        unsigned int localDummyValue;
        while (bucketsInitialized == 0) {
            // Prevents the while(true) loop from being optimized out of the compiled code
            # pragma omp atomic
            localDummyValue++;
        }


    }
};

class StreamingSubset : public MutableSubset {
    private:
    std::unique_ptr<MutableSubset> base;
    const LocalData &data;
    std::vector<MPI_Request> sends;

    public:
    StreamingSubset(const LocalData& data) : data(data), base(NaiveMutableSubset::makeNew()) {}

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
        MPI_Isend(rowToSend.data(), rowToSend.size(), MPI_DOUBLE, 0, data.getRemoteIndexForRow(row), MPI_COMM_WORLD, request);
        this->base->addRow(row, marginalGain);
    }
};