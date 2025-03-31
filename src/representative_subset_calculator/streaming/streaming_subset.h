
#include <queue>
#include <future>
#include <optional>
#include <chrono>

#include "../../data_tools/base_data.h"
#include "../representative_subset.h"
#include "../timers/timers.h"

#ifndef STREAMING_SUBSET_H
#define STREAMING_SUBSET_H

class StreamingSubset : public MutableSubset {
    private:
    const BaseData &data;
    Timers &timers;
    
    std::vector<std::unique_ptr<MpiSendRequest>> sends;
    std::unique_ptr<MutableSubset> delegate;

    const unsigned int seedsToSend;
    unsigned int seedsSent;

    public:
    StreamingSubset(
        const BaseData& data, 
        std::unique_ptr<MutableSubset> delegate,
        Timers &timers,
        const unsigned int seedsToSend
    ) : 
        data(data), 
        delegate(std::move(delegate)),
        timers(timers),
        seedsToSend(seedsToSend)
    {
        timers.firstSeedTime.startTimer();
    }

    float getScore() const {
        return delegate->getScore();
    }

    size_t getRow(const size_t index) const {
        return delegate->getRow(index);
    }

    size_t size() const {
        return delegate->size();
    }

    nlohmann::json toJson() const {
        return delegate->toJson();
    }

    const size_t* begin() const {
        return delegate->begin();
    }

    const size_t* end() const {
        return delegate->end();
    }

    void finalize() {
        this->timers.communicationTime.startTimer();
        for (size_t i = 0; i < this->sends.size() - 1; i++) {
            MpiSendRequest *send = this->sends[i].get();
            send->waitForISend();
        }

        // Without this, the global receiver would have no way of knowing when
        // to stop receiving
        SPDLOG_DEBUG("sending stop metadata");
        std::vector<float> localSubset;
        for (size_t r : *this->delegate) {
            localSubset.push_back(this->data.getRemoteIndexForRow(r));
        }

        // Include total marginal gain
        localSubset.push_back(this->delegate->getScore());
        
        localSubset.push_back(CommunicationConstants::endOfSendTag());

        std::unique_ptr<MpiSendRequest> send = std::unique_ptr<MpiSendRequest>(new MpiSendRequest(std::move(localSubset)));
        send->isend(CommunicationConstants::getStopTag());
        send->waitForISend();
        this->timers.communicationTime.stopTimer();
    }

    void addRow(const size_t row, const float marginalGain) {
        this->delegate->addRow(row, marginalGain);

        if (this->delegate->size() <= this->seedsToSend) {
            ToBinaryVisitor visitor;
            std::vector<float> rowToSend(std::move(this->data.getRow(row).visit(visitor)));

            // last value should be the global row index
            rowToSend.push_back(this->data.getRemoteIndexForRow(row));

            // Mark the end of the send buffer
            rowToSend.push_back(CommunicationConstants::endOfSendTag());

            if(this->delegate->size() == 1) {
                timers.firstSeedTime.stopTimer();
            }

            this->sends.push_back(std::unique_ptr<MpiSendRequest>(new MpiSendRequest(std::move(rowToSend))));

            // Since we are now sending a summary of the local subset after finding all seeds, we
            // can just send seeds as we find them. We do not need to wait to send the first seed
            bool sendingLastSeed = this->delegate->size() == this->seedsToSend;
            const int tag = CommunicationConstants::getContinueTag();
            this->sends.back()->isend(tag);
        }
    }
};

#endif