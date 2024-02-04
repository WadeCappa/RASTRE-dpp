#include <vector>

class RepresentativeSubset {
    public:
    virtual double getScore() const = 0;
    virtual std::vector<size_t> getRows() const = 0;
    virtual size_t getNumberOfRows() const = 0;
};

class DummyRepresentativeSubset : public RepresentativeSubset {
    private:
    double score;
    std::vector<size_t> rows;

    public:
    DummyRepresentativeSubset(const std::vector<size_t> &rows, const double score) : rows(rows), score(score) {}

    double getScore() const {
        return score;
    }

    std::vector<size_t> getRows() const {
        return rows;
    }

    size_t getNumberOfRows() const {
        return this->rows.size();
    }
};

class NaiveRepresentativeSubset : public RepresentativeSubset {
    private:
    double score;
    std::vector<size_t> rows;

    void setState(const std::vector<std::pair<size_t, double>> &res) {
        this->score = 0;
        for (const auto & s : res) {
            this->score += s.second;
            this->rows.push_back(s.first);
        }
    }

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
        this->setState(res);
    }

    NaiveRepresentativeSubset(
        // This calculator will be mutated, should never be re-used.
        std::unique_ptr<RepresentativeSubsetCalculator> calculator, 
        const SelectiveData &data,
        const size_t desiredRows,
        Timers &timers
    ) {
        timers.globalCalculationTime.startTimer();
        auto res = calculator->getApproximationSet(data, desiredRows);
        timers.globalCalculationTime.stopTimer();
        this->setState(data.translateSolution(res));
    }

    double getScore() const {
        return score;
    }

    std::vector<size_t> getRows() const {
        return rows;
    }

    size_t getNumberOfRows() const {
        return this->rows.size();
    }
};