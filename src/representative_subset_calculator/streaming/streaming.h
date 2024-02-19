#include "bucket.h"
#include <mpi.h>

static const int DOUBLES_FOR_ROW_INDEX = 1;

class StreamingReceiver {
    public:
    virtual std::unique_ptr<Subset> stream() = 0;
};

class SeiveStreamingReceiver : public StreamingReceiver {
    private:
    const int totalReceiveBufferSize;

    struct ReceiveBuffers {
        private:
        struct MaybePoplulatedBuffer {
            std::vector<double> buffer;
            MPI_Request request;
            MaybePoplulatedBuffer(const size_t bufferSize) : buffer(bufferSize) {}
        };
        
        std::vector<MaybePoplulatedBuffer> buffers;
        std::vector<size_t> rankToCurrentSeed;

        MaybePoplulatedBuffer &getBuffer(const int rank, const int seed) {
            return buffers[(rank * rankToCurrentSeed.size()) + seed];
        }

        public:
        ReceiveBuffers(const int worldSize, const size_t rowSize, const int threads, const int k) 
        : 
            buffers(worldSize * k, MaybePoplulatedBuffer(rowSize + DOUBLES_FOR_ROW_INDEX)),
            rankToCurrentSeed(worldSize, 0)
        {
            for (size_t rank = 0; rank < worldSize; rank++) {
                for (size_t seed = 0; seed < k; seed++) {
                    MaybePoplulatedBuffer &buffer(getBuffer(rank, seed));
                    MPI_Irecv(
                        buffer.buffer.data(), buffer.buffer.size(), 
                        MPI_DOUBLE, rank, seed, 
                        MPI_COMM_WORLD, &buffer.request
                    );
                }
            }
        }

        const std::vector<double> &receiveNextSeed() {
            while (true) {
                for (size_t rank = 0; rank < this->rankToCurrentSeed.size(); rank++) {
                    const size_t currentSeed = this->rankToCurrentSeed[rank];
                    int flag;
                    MPI_Status status;
                    MPI_Test(&(getBuffer(rank, currentSeed).request), &flag, &status);
                    if (flag == 1) {
                        this->rankToCurrentSeed[rank]++;
                        return getBuffer(rank, currentSeed).buffer;
                    }
                }
            }
        }
    };

    ReceiveBuffers buffers;
    std::vector<ThresholdBucket> buckets;

    public:
    SeiveStreamingReceiver(const int worldSize, const size_t rowSize, const int threads, const int k) 
    : 
        totalReceiveBufferSize(rowSize + DOUBLES_FOR_ROW_INDEX),
        buffers(worldSize, rowSize + DOUBLES_FOR_ROW_INDEX, threads, k)
    {}
    
    std::unique_ptr<Subset> stream() {

    }

    private:
    void listenForSeeds() {
        while (true) {
            const std::vector<double> &data = buffers.receiveNextSeed();
        } 
    }

    void processSeeds() {

    }
};

class StreamingSubset : public MutableSubset {
    private:
    std::unique_ptr<MutableSubset> base;

    public:
    StreamingSubset() : base(NaiveMutableSubset::makeNew()) {}

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
        // Overload this for streaming. When something has been added, send to the global node.
    }
};