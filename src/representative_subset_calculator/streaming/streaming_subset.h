#include <queue>
#include <future>
#include <optional>
#include <chrono>

class StreamingSubset : public MutableSubset {
    private:
    std::unique_ptr<MutableSubset> base;
    const LocalData &data;
    std::queue<std::unique_ptr<MpiSendRequest>> sends;

    const unsigned int desiredSeeds;

    public:
    StreamingSubset(
        const LocalData& data, 
        const unsigned int desiredSeeds
    ) : 
        data(data), 
        base(NaiveMutableSubset::makeNew()),
        desiredSeeds(desiredSeeds)
    {}

    void sendRemaining() {
        // block and send remaining
    }

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

    void finalize() {
        while (this->sends.size() > 0) {
            if (this->sends.front()->sendStarted()) {
                this->sends.front()->waitForCompletion();
                this->sends.pop();
            } else {
                this->sends.front()->sendAndBlock(
                    this->sends.size() == 1 ? CommunicationConstants::getStopTag() : CommunicationConstants::getContinueTag()
                );
                this->sends.pop();
            }
        }
    }

    void addRow(const size_t row, const double marginalGain) {
        this->base->addRow(row, marginalGain);

        std::vector<double> rowToSend(this->data.getRow(row));

        // Last value should be the global row index
        rowToSend.push_back(data.getRemoteIndexForRow(row));

        this->sends.push(std::unique_ptr<MpiSendRequest>(new MpiSendRequest(rowToSend)));

        if (sends.front()->sendCompleted()) {
            sends.pop();
        }

        // Keep at least one seed until finalize is called. Otherwise there is no garuntee of
        //  how many seeds there are left to find.
        if (!(sends.front()->sendStarted()) && sends.size() > 1) {
            // Tag should be -1 iff this is the last seed to be sent. This code purposefully
            //  does not send the last seed until finalize is called
            const int tag = CommunicationConstants::getContinueTag();
            
            // TODO: This method should have an async callback to queue the next send. Will require locks
            sends.front()->sendAsync(tag);
        }
    }
};
