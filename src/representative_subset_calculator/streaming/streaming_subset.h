#include <queue>
#include <future>
#include <optional>
#include <chrono>

class StreamingSubset : public MutableSubset {
    private:
    const SegmentedData &data;
    Timers &timers;
    
    std::vector<std::unique_ptr<MpiSendRequest>> sends;
    std::unique_ptr<MutableSubset> base;

    const unsigned int seedsToSend;
    unsigned int seedsSent;

    public:
    StreamingSubset(
        const SegmentedData& data, 
        Timers &timers,
        const unsigned int seedsToSend
    ) : 
        data(data), 
        base(NaiveMutableSubset::makeNew()),
        timers(timers),
        seedsToSend(seedsToSend)
    {
        timers.firstSeedTime.startTimer();
    }

    float getScore() const {
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
        for (size_t r : *this->base) {
            localSubset.push_back(this->data.getRemoteIndexForRow(r));
        }

        // Include total marginal gain
        localSubset.push_back(this->base->getScore());

        std::unique_ptr<MpiSendRequest> send = std::unique_ptr<MpiSendRequest>(new MpiSendRequest(move(localSubset)));
        send->waitForISend();
        this->timers.communicationTime.stopTimer();
    }

    void addRow(const size_t row, const float marginalGain) {
        this->base->addRow(row, marginalGain);

        if (this->base->size() <= this->seedsToSend) {
            ToBinaryVisitor visitor;
            std::vector<float> rowToSend(move(this->data.getRow(row).visit(visitor)));

            // second to last value should be the marginal gain of this element for the local solution
            rowToSend.push_back(marginalGain);

            // last value should be the global row index
            rowToSend.push_back(this->data.getRemoteIndexForRow(row));

            // Mark the end of the send buffer
            rowToSend.push_back(CommunicationConstants::endOfSendTag());

            if(this->base->size() == 1) {
                timers.firstSeedTime.stopTimer();
            }

            this->sends.push_back(std::unique_ptr<MpiSendRequest>(new MpiSendRequest(move(rowToSend))));

            // Since we are now sending a summary of the local subset after finding all seeds, we
            // can just send seeds as we find them. We do not need to wait to send the first seed
            bool sendingLastSeed = this->base->size() == this->seedsToSend;
            const int tag = sendingLastSeed ? 
                CommunicationConstants::getStopTag() : 
                CommunicationConstants::getContinueTag();
            if (sendingLastSeed) {
                SPDLOG_DEBUG("Sending last seed");
            }
            this->sends.back()->isend(tag);
        }
    }
};
