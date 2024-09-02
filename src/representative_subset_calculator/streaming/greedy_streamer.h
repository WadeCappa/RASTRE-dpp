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
    const bool continueAcceptingSeedsAfterFillingBuckets;

    SynchronousQueue<std::unique_ptr<CandidateSeed>> queue;

    public:
    SeiveGreedyStreamer(
        Receiver &receiver, 
        CandidateConsumer &consumer,
        Timers &timers,
        const bool continueAcceptingSeedsAfterFillingBuckets
    ) : 
        receiver(receiver),
        consumer(consumer),
        timers(timers),
        continueAcceptingSeedsAfterFillingBuckets(continueAcceptingSeedsAfterFillingBuckets)
    {}

    std::unique_ptr<Subset> resolveStream() {
        resolveStreamInternal();

        std::cout << "getting best consumer, destroying in process" << std::endl;
        
        std::unique_ptr<Subset> bestLocalSolution(receiver.getBestReceivedSolution());
        std::unique_ptr<Subset> streamingSolution(consumer.getBestSolutionDestroyConsumer());

        std::cout << "streaming solution has score of " << streamingSolution->getScore() << std::endl;
        std::cout << "best local solution has score of " << bestLocalSolution->getScore() << std::endl;

        return bestLocalSolution->getScore() > streamingSolution->getScore() ? move(bestLocalSolution) : move(streamingSolution);
    }

    private:
    // TODO: One more send then required is accepted after `stillConsuming` is false. There
    //  should be a way to exit even earlier here.
    void resolveStreamInternal() {
        unsigned int dummyVal = 0;
        std::atomic_bool stillReceiving = true;
        std::atomic_bool stillConsuming = true;
        omp_set_dynamic(0);
        omp_set_nested(1);
        #pragma omp parallel num_threads(2)
        {
            int threadId = omp_get_thread_num();
            if (threadId == 0) {
                timers.communicationTime.startTimer();

                while (stillReceiving.load() && stillConsuming.load()) {
                    std::unique_ptr<CandidateSeed> nextSeed(receiver.receiveNextSeed(stillReceiving));
                    this->queue.push(move(nextSeed));
                }

                timers.communicationTime.stopTimer();
            } else {
                while (stillReceiving.load() && stillConsuming.load()) {
                    timers.waitingTime.startTimer();
                    while (this->queue.isEmpty())  {
                        dummyVal++;
                    }
                    timers.waitingTime.stopTimer();

                    // If continueAcceptingSeedsAfterFillingBuckets is false, this should always be true so we don't stop
                    // 	receiving seeds.
                    bool consumerIsStillReceiving = consumer.accept(this->queue, timers);
                    stillConsuming.store(consumerIsStillReceiving || continueAcceptingSeedsAfterFillingBuckets);
                }

                // Queue may still have elements after receiver signals to stop streaming
                consumer.accept(this->queue, timers);
            }
        }
    }
};
