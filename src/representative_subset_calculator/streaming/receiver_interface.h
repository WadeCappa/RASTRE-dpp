#include <atomic>

#include "candidate_seed.h"
#include "../representative_subset.h"

#ifndef RECEIVER_INTERFACE_H
#define RECEIVER_INTERFACE_H

class Receiver {
    public:
    virtual ~Receiver() {}

    // N.B. this method should never return a nullptr. The greedy streamer will break if it sees one.
    virtual std::unique_ptr<CandidateSeed> receiveNextSeed(std::atomic_bool &stillReceiving) = 0;
    virtual std::unique_ptr<Subset> getBestReceivedSolution() = 0;
};

#endif