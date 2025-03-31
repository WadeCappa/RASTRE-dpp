
#include <omp.h>
#include <atomic>

#include "candidate_consumer.h"
#include "../representative_subset.h"
#include "candidate_seed.h"
#include "../timers/timers.h"
#include "receiver_interface.h"

#ifndef GREEDY_STREAMER_H
#define GREEDY_STREAMER_H

class GreedyStreamer {
    public:
    virtual ~GreedyStreamer() {}
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

        std::unique_ptr<Subset> bestLocalSolution(receiver.getBestReceivedSolution());
        std::unique_ptr<Subset> streamingSolution(consumer.getBestSolutionDestroyConsumer());

        spdlog::info("streaming solution has score of {0:f} and size of {1:d}", streamingSolution->getScore(), streamingSolution->size());
        spdlog::info("best local solution has score of {0:f} and size of {1:d}", bestLocalSolution->getScore(), bestLocalSolution->size());

        return bestLocalSolution->getScore() > streamingSolution->getScore() ? move(bestLocalSolution) : move(streamingSolution);
    }

    private:
    /**
     * N.B. Stop early has been disabled since it was causing segfaults and is technically incorrect. The code 
     * for stop-early has not been entirly removed.
     */
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
                    if (!stillReceiving.load()) {
                        break;
                    }
                    if (nextSeed != nullptr) { 
                        this->queue.push(move(nextSeed));
                    } else {
                        SPDLOG_DEBUG("received nullptr");
                    }
                }
                SPDLOG_DEBUG("receiver is no longer waiting for data");
                timers.communicationTime.stopTimer();
            } else {
                while (stillReceiving.load() && stillConsuming.load()) {
                    timers.waitingTime.startTimer();
                    while (this->queue.isEmpty() && stillReceiving.load() && stillConsuming.load())  {
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

#endif