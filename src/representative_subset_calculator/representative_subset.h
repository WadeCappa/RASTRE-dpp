#include <vector>
#include <nlohmann/json.hpp>

class Subset {
    public:
    virtual double getScore() const = 0;
    virtual size_t getRow(const size_t index) const = 0;
    virtual size_t size() const = 0;
    virtual nlohmann::json toJson() const = 0;
    virtual const size_t* begin() const = 0;
    virtual const size_t* end() const = 0;

    static std::unique_ptr<Subset> of(const std::vector<size_t> &rows, const double score);
    static std::unique_ptr<Subset> empty();
};

class MutableSubset : public Subset {
    private:
    double score;
    std::vector<size_t> rows;

    public:
    MutableSubset() : score(0), rows(std::vector<size_t>()) {}
    MutableSubset(const std::vector<size_t> &rows, const double score) : rows(rows), score(score) {}

    void addRow(const size_t row, const double marginalGain) {
        this->rows.push_back(row);
        this->score += marginalGain;
    }

    static std::unique_ptr<Subset> upcast(MutableSubset* mutableSubset) {
        Subset* immutableSubset = dynamic_cast<Subset*>(mutableSubset);
        return std::unique_ptr<Subset>(immutableSubset);
    }

    double getScore() const {
        return score;
    }

    size_t getRow(const size_t index) const {
        return this->rows[index];
    }

    size_t size() const {
        return this->rows.size();
    }

    const size_t* begin() const {
        return this->rows.data();
    }

    const size_t* end() const {
        return this->rows.data() + this->rows.size();
    }

    nlohmann::json toJson() const {
        nlohmann::json output {
            {"rows", this->rows}, 
            {"totalCoverage", this->score}
        };

        return output;
    }
};

std::unique_ptr<Subset> Subset::of(
    const std::vector<size_t> &rows, 
    const double score
) {
    Subset* subset = dynamic_cast<Subset*>(new MutableSubset(rows, score));
    return std::unique_ptr<Subset>(subset);
}

std::unique_ptr<Subset> Subset::empty() {
    std::vector<size_t> emptyRows;
    return Subset::of(emptyRows, 0);
}

// class NaiveSubset : public Subset {
//     private:
//     double score;
//     std::vector<size_t> rows;

//     void setState(const std::vector<std::pair<size_t, double>> &res) {
//         this->score = 0;
//         for (const auto & s : res) {
//             this->score += s.second;
//             this->rows.push_back(s.first);
//         }
//     }

//     public:
//     NaiveSubset(
//         // This calculator will be mutated, should never be re-used.
//         std::unique_ptr<SubsetCalculator> calculator, 
//         const Data &data,
//         const size_t desiredRows,
//         Timers &timers
//     ) {
//         timers.localCalculationTime.startTimer();
//         auto res = calculator->getApproximationSet(data, desiredRows);
//         timers.localCalculationTime.stopTimer();
//         this->setState(res);
//     }

//     NaiveSubset(
//         // This calculator will be mutated, should never be re-used.
//         std::unique_ptr<SubsetCalculator> calculator, 
//         const SelectiveData &data,
//         const size_t desiredRows,
//         Timers &timers
//     ) {
//         timers.globalCalculationTime.startTimer();
//         auto res = calculator->getApproximationSet(data, desiredRows);
//         timers.globalCalculationTime.stopTimer();
//         this->setState(data.translateSolution(res));
//     }

//     double getScore() const {
//         return score;
//     }

//     std::vector<size_t> getRows() const {
//         return rows;
//     }

//     size_t size() const {
//         return this->rows.size();
//     }
// };