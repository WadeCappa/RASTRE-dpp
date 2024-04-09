#include <queue>
#include <future>
#include <optional>
#include <chrono>

class StreamingSubset : public MutableSubset {
    private:
    class ToBinaryVisitor : public DataRowVisitor {
        std::vector<double> binary;

        public:
        void visitDenseDataRow(const std::vector<double>& data) {
            binary = data;
        }

        void visitSparseDataRow(const std::map<size_t, double>& data, size_t totalColumns) {
            size_t index = 0;
            for (const auto & p : data) {
                binary[index++] = p.first;
                binary[index++] = p.second;
            }

            binary[index] = CommunicationConstants::getNoMoreEdgesTag();
        }

        std::vector<double> getAndDestroy() {
            return move(binary);
        }
    };

    const SegmentedData &data;
    Timers &timers;
    
    std::vector<std::unique_ptr<MpiSendRequest>> sends;
    std::unique_ptr<MutableSubset> base;

    const unsigned int desiredSeeds;

    public:
    StreamingSubset(
        const SegmentedData& data, 
        const unsigned int desiredSeeds,
        Timers &timers
    ) : 
        data(data), 
        base(NaiveMutableSubset::makeNew()),
        desiredSeeds(desiredSeeds),
        timers(timers)
    {
        timers.firstSeedTime.startTimer();
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
        this->timers.communicationTime.startTimer();
        for (size_t i = 0; i < this->sends.size() - 1; i++) {
            MpiSendRequest *send = this->sends[i].get();
            send->waitForISend();
        }
        
        // Can only queue the last send after all other sends have been sent. Otherwise 
        //  this will cause a race condition causing senders to sometimes never finish 
        //  sending seeds.
        this->sends.back()->isend(CommunicationConstants::getStopTag());
        this->sends.back()->waitForISend();
        this->timers.communicationTime.stopTimer();
    }

    void addRow(const size_t row, const double marginalGain) {
        this->base->addRow(row, marginalGain);

        ToBinaryVisitor visitor;
        this->data.getRow(row).visit(visitor);
        std::vector<double> rowToSend(move(visitor.getAndDestroy()));

        // second to last value should be the marginal gain of this element for the local solution
        rowToSend.push_back(marginalGain);

        // last value should be the global row index
        rowToSend.push_back(data.getRemoteIndexForRow(row));

        if(this->base->size() == 1)
            timers.firstSeedTime.stopTimer();

        this->sends.push_back(std::unique_ptr<MpiSendRequest>(new MpiSendRequest(move(rowToSend))));

        // Keep at least one seed until finalize is called. Otherwise there is no garuntee of
        //  how many seeds there are left to find.
        if (sends.size() > 1) {
            // Tag should be -1 iff this is the last seed to be sent. This code purposefully
            //  does not send the last seed until finalize is called
            const int tag = CommunicationConstants::getContinueTag();
            sends[this->sends.size() - 2]->isend(tag);
        }
    }
};
