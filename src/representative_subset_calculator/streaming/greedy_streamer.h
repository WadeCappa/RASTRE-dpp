#include <omp.h>
#include <atomic>

class GreedyStreamer {
    public:
    virtual std::unique_ptr<Subset> resolveStream() = 0;
};

class SeiveGreedyStreamer : public GreedyStreamer {
    private:
    Receiver &receiver;
    CandidateConsumer &consumer;

    SynchronousQueue<std::unique_ptr<CandidateSeed>> queue;

    public:
    SeiveGreedyStreamer(
        Receiver &receiver, 
        CandidateConsumer &consumer
    ) : 
        receiver(receiver),
        consumer(consumer)
    {}


    std::unique_ptr<Subset> resolveStream() {
        std::atomic_bool stillReceiving = true;
        omp_set_dynamic(0);
        omp_set_nested(1);
        #pragma omp parallel num_threads(2)
        {
            int threadId = omp_get_thread_num();
            if (threadId == 0) {
                while (stillReceiving.load() == true) {
                    std::unique_ptr<CandidateSeed> nextSeed(receiver.receiveNextSeed(stillReceiving));
                    this->queue.push(move(nextSeed));
                }
            } else {
                while (stillReceiving.load() == true) {
                    consumer.accept(this->queue);
                }
            }
        }

        consumer.accept(this->queue);

        std::cout << "getting best consumer, destroying in process" << std::endl;
        return consumer.getBestSolutionDestroyConsumer();
    }
};