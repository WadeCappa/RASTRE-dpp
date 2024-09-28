

class LoadingReceiver : public Receiver {
    private:
    std::unique_ptr<DataRowFactory> factory;
    std::unique_ptr<LineFactory> getter;
    std::unique_ptr<DataRow> lastRow;

    size_t globalRow;

    public:
    LoadingReceiver(
        std::unique_ptr<DataRowFactory> factory, 
        std::unique_ptr<LineFactory> getter
    ) : factory(move(factory)), getter(move(getter)), lastRow(nullptr), globalRow(0) {}

    std::unique_ptr<CandidateSeed> receiveNextSeed(std::atomic_bool &stillReceiving) {
        std::unique_ptr<DataRow> nextRow(factory->maybeGet(*getter));

        if (nextRow == nullptr) {
            stillReceiving.store(false);
        }

        std::unique_ptr<CandidateSeed> result(new CandidateSeed(globalRow++, move(lastRow), 1));
        lastRow = move(nextRow);
        return move(result);
    }

    std::unique_ptr<Subset> getBestReceivedSolution() {
        return Subset::empty();
    }
};