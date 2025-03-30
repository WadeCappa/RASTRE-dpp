

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

/**
 * Only for use while loading the dataset while calculating. Otherwise we just won't be sending these values to
 * the global calculator so we really don't need to worry about this
 */
class UserModeReceiver : public Receiver {
    private:
    std::unique_ptr<Receiver> delegate;
    const UserData & user;
    const std::unordered_set<unsigned long long> user_set;

    UserModeReceiver(
        std::unique_ptr<Receiver> delegate,
        const UserData& user,
        const std::unordered_set<unsigned long long> user_set
    ) : 
        delegate(move(delegate)),
        user(user),
        user_set(user_set)
    {}

    public:
    static std::unique_ptr<UserModeReceiver> create(
        std::unique_ptr<Receiver> delegate, 
        const UserData &user
    ) {
        std::unordered_set<unsigned long long> user_set(
            user.getCu().begin(),
            user.getCu().end());
        return std::unique_ptr<UserModeReceiver>(new UserModeReceiver(move(delegate), user, move(user_set)));
    }

    std::unique_ptr<CandidateSeed> receiveNextSeed(std::atomic_bool &stillReceiving) {
        while (true) {
            std::unique_ptr<CandidateSeed> next_seed(delegate->receiveNextSeed(stillReceiving));
            if (stillReceiving.load() == false) {
                // always return the last seed since this is *supposed* to be empty
                return next_seed;
            }

            if (user_set.find(next_seed->getRow()) != user_set.end()) {
                return move(next_seed);
            }
        }
    }

    std::unique_ptr<Subset> getBestReceivedSolution() {
        return move(delegate->getBestReceivedSolution());
    }
};