
class RelevanceCalculatorFactory {
    public:
    virtual ~RelevanceCalculatorFactory() {}
    virtual std::unique_ptr<RelevanceCalculator> build(const BaseData& d) const = 0;
};

class NaiveRelevanceCalculatorFactory : public RelevanceCalculatorFactory {
    public:
    std::unique_ptr<RelevanceCalculator> build(const BaseData& d) const {
        return NaiveRelevanceCalculator::from(d);
    }
};

class UserModeNaiveRelevanceCalculatorFactory : public RelevanceCalculatorFactory {
    private:
    const UserData& user;
    const double theta;

    public:
    UserModeNaiveRelevanceCalculatorFactory(
        const UserData& user,
        const double theta
    ) : user(user), theta(theta) {}

    std::unique_ptr<RelevanceCalculator> build(const BaseData& d) const {
        return UserModeRelevanceCalculator::from(d, user, theta);
    }
};

class PerRowRelevanceCalculator {
    private:
    class DummyData : public BaseData {
        private:
        const DataRow &row;

        public:
        DummyData(const DataRow& row) : row(row) {}

        const DataRow& getRow(size_t _i) const {
            return row;
        }

        size_t totalRows() const {
            return 1;
        }

        size_t totalColumns() const {
            return row.size();
        }

        /**
         * Not sure if this is safe to call, might want to throw here
         */
        size_t getRemoteIndexForRow(const size_t localRowIndex) const {
            spdlog::error("Not sure if its safe to call this, we might want to throw here instead");
            return localRowIndex;
        }

        size_t getLocalIndexFromGlobalIndex(const size_t globalIndex) const {
            spdlog::error("Not sure if its safe to call this, we might want to throw here instead");
            return globalIndex;
        }
    };

    public:
    static float getScore(const DataRow &row, const RelevanceCalculatorFactory &factory) {
        DummyData data(row);
        return factory.build(data)->get(0, 0);
    }
};
