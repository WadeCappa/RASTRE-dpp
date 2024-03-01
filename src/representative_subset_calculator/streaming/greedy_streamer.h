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
    Timers &timers;

    SynchronousQueue<std::unique_ptr<CandidateSeed>> queue;

    public:
    SeiveGreedyStreamer(
        Receiver &receiver, 
        CandidateConsumer &consumer,
        Timers &timers
    ) : 
        receiver(receiver),
        consumer(consumer),
        timers(timers)
    {}


    std::unique_ptr<Subset> resolveStream() {
        std::atomic_bool stillReceiving = true;
        omp_set_dynamic(0);
        omp_set_nested(1);
        #pragma omp parallel num_threads(2)
        {
            int threadId = omp_get_thread_num();
            if (threadId == 0) {
                timers.receiverTime.startTimer();

                while (stillReceiving.load() == true) {
                    std::unique_ptr<CandidateSeed> nextSeed(receiver.receiveNextSeed(stillReceiving));
                    this->queue.push(move(nextSeed));
                }

                timers.receiverTime.stopTimer();
            } else {
                timers.consumerTime.startTimer();

                while (stillReceiving.load() == true) {
                    consumer.accept(this->queue);
                }

                // Queue may still have elements after receiver signals to stop streaming
                consumer.accept(this->queue);
            
                timers.consumerTime.stopTimer();
            }
        }

        std::cout << "getting best consumer, destroying in process" << std::endl;
        
        std::unique_ptr<Subset> bestLocalSolution(receiver.getBestReceivedSolution());
        std::unique_ptr<Subset> streamingSolution(consumer.getBestSolutionDestroyConsumer());

        std::cout << "streaming solution has score of " << streamingSolution->getScore() << std::endl;
        std::cout << "best local solution has score of " << bestLocalSolution->getScore() << std::endl;

        return bestLocalSolution->getScore() > streamingSolution->getScore() ? move(bestLocalSolution) : move(streamingSolution);
    }
};