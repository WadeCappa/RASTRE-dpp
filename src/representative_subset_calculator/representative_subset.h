#include <vector>

class RepresentativeSubset {
    public:
    virtual double getScore() const = 0;
    virtual std::vector<size_t> getRows() const = 0;
};

class NaiveRepresentativeSubset : public RepresentativeSubset {
    private:
    double score;
    std::vector<size_t> rows;

    public:
    NaiveRepresentativeSubset(
        // This calculator will be mutated, should never be re-used.
        std::unique_ptr<RepresentativeSubsetCalculator> calculator, 
        const Data &data,
        const size_t desiredRows,
        Timers &timers
    ) {
        timers.localCalculationTime.startTimer();
        auto res = calculator->getApproximationSet(data, desiredRows);
        timers.localCalculationTime.stopTimer();
        this->score = 0;
        for (const auto & s : res) {
            this->score += s.second;
            this->rows.push_back(s.first);
        }
    }

    double getScore() const {
        return score;
    }

    std::vector<size_t> getRows() const {
        return rows;
    }
};