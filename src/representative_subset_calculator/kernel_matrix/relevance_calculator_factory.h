
#include "../../data_tools/base_data.h"
#include "../../user_mode/user_data.h"
#include "relevance_calculator.h"

#ifndef RELEVANCE_CALCULATOR_FACTOR_H
#define RELEVANCE_CALCULATOR_FACTOR_H

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
        const std::unordered_map<unsigned long long, double>& globalRowToRu(
            user.getCuToRuMapping()
        );

        std::vector<double> relativeRu;
        for (size_t s = 0; s < d.totalRows(); s++) {
            size_t globalRow = d.getRemoteIndexForRow(s);
            SPDLOG_TRACE("looking at {0:d} for local of {1:d}", globalRow, s);
            relativeRu.push_back(globalRowToRu.at(globalRow));
        }

        return UserModeRelevanceCalculator::from(d, std::move(relativeRu), theta);
    }
};

class PerRowRelevanceCalculator {
    private:
    class DummyData : public BaseData {
        private:
        const DataRow &row;
        const size_t globalRow;

        public:
        DummyData(const DataRow& row, const size_t globalRow) 
        : row(row), globalRow(globalRow) {}

        const DataRow& getRow(size_t _i) const {
            return row;
        }

        size_t totalRows() const {
            return 1;
        }

        size_t totalColumns() const {
            return row.size();
        }

        size_t getRemoteIndexForRow(const size_t localRowIndex) const {
            if (localRowIndex != 0) {
                spdlog::error("This dummy data object only has one row, cannot return mapping for {0:d}", localRowIndex);
            }
            return globalRow;
        }

        size_t getLocalIndexFromGlobalIndex(const size_t globalIndex) const {
            if (globalIndex != globalRow) {
                spdlog::error("Unrecognized global row of {0:d} but expected {1:d}", globalIndex, globalRow);
            }

            return globalRow;
        }
    };

    public:
    static float getScore(
        const DataRow &row, 
        const RelevanceCalculatorFactory &factory,
        const size_t globalRow
    ) {
        DummyData data(row, globalRow);
        return factory.build(data)->get(0, 0);
    }
};

#endif