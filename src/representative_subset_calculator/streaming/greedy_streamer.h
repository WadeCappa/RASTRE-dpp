#include "bucket.h"
#include <mpi.h>
#include <queue>

class GreedyStreamer {
    public:
    virtual std::unique_ptr<Subset> resolveStream();
};

class SeiveGreedyStreamer : public GreedyStreamer {
    private:
    Receiver &receiver;
    CandidateConsumer &consumer;

    public:
    SeiveGreedyStreamer(Reciever &receiver, CandidateConsumer &consumer)
    : 
        receiver(receiver),
        consumer(consumer)
    {}
    
    std::unique_ptr<Subset> resolveStream(const int threads) {
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

            if (shouldInitializeBuckets && bucketsInitialized == 0) {
                # pragma omp atomic
                bucketsInitialized++;
            }
        }
    }

    void processSeeds(const int threadsForProcessing) {
        unsigned int localDummyValue;
        while (bucketsInitialized == 0) {
            // Prevents the while(true) loop from being optimized out of the compiled code
            # pragma omp atomic
            localDummyValue++;
        }

        // init buckets first

        // process in 
    }

    void initializeBuckets() {
       this->deltaZero = deltaZero;

        int num_buckets = (int)(0.5 + [](double val, double base) {
            return log2(val) / log2(base);
        }((double)k, (1+this->epsilon)));

        // std::cout << "deltazero: " << this->deltaZero << ", number of buckets: " << num_buckets << std::endl;

        for (int i = 0; i < num_buckets + 1; i++)
        {
            this->buckets.push_back(new ThresholdBucket(this->deltaZero, this->k, this->epsilon, i));
        }
    }
};