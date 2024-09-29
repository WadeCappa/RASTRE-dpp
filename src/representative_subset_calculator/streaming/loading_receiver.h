

class LoadingReceiver : public Receiver {
    private:
    std::unique_ptr<DataRowFactory> factory;
    std::unique_ptr<LineFactory> getter;

    size_t globalRow;
    size_t knownColumns;

    public:
    LoadingReceiver(
        std::unique_ptr<DataRowFactory> factory, 
        std::unique_ptr<LineFactory> getter
    ) : factory(move(factory)), getter(move(getter)), globalRow(0), knownColumns(0) {}

    std::unique_ptr<CandidateSeed> receiveNextSeed(std::atomic_bool &stillReceiving) {
        std::unique_ptr<DataRow> nextRow(factory->maybeGet(*getter));

        if (nextRow == nullptr) {
            stillReceiving.store(false);
            
            // Returning an empty row here is likely fine. Since this is the last element that the consumer 
            //  will see this won't affect the solution.
            std::unique_ptr<DataRow> emptyRow (new SparseDataRow(std::map<size_t, float>(), knownColumns));
            return std::unique_ptr<CandidateSeed>(new CandidateSeed(globalRow++, move(emptyRow), 1));
        }

        // This is a little bit of a hack, it would be better to just pass in the number of expected columns 
        //  into this class isntead of tracking like this.
        knownColumns = nextRow->size();

        return std::unique_ptr<CandidateSeed>(new CandidateSeed(globalRow++, move(nextRow), 1));
    }

    std::unique_ptr<Subset> getBestReceivedSolution() {
        return Subset::empty();
    }
};