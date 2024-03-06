#include <vector>
#include <memory>

class ThresholdBucket 
{
    private:
    std::unique_ptr<MutableSubset> solution;
    std::unique_ptr<std::vector<const std::vector<double> *>> solutionRows; 
    std::unique_ptr<std::vector<double>> d; 
    std::unique_ptr<std::vector<std::vector<double>>> b; 

    double threshold;
    int k;

    public:
    ThresholdBucket(const double threshold, const int k) 
    : 
        threshold(threshold), 
        k(k), 
        solution(NaiveMutableSubset::makeNew()), 
        solutionRows(std::make_unique<std::vector<const std::vector<double> *>>()),
        d(std::make_unique<std::vector<double>>()),
        b(std::make_unique<std::vector<std::vector<double>>>())
    {}

    ThresholdBucket(
        const double threshold, 
        const int k, 
        std::unique_ptr<MutableSubset> nextSolution,
        std::unique_ptr<std::vector<const std::vector<double> *>> solutionRows,
        std::unique_ptr<std::vector<double>> d,
        std::unique_ptr<std::vector<std::vector<double>>> b
    ) : 
        threshold(threshold), 
        k(k), 
        solution(move(nextSolution)),
        solutionRows(move(solutionRows)),
        d(move(d)),
        b(move(b))
    {}

    std::unique_ptr<ThresholdBucket> transferContents(const double newThreshold) {
        return std::unique_ptr<ThresholdBucket>(new ThresholdBucket(newThreshold, this->k, move(this->solution), move(this->solutionRows), move(this->d), move(this->b)));
    }

    size_t getUtility() {
        return this->solution->getScore();
    }

    std::unique_ptr<Subset> returnSolutionDestroyBucket() {
        return MutableSubset::upcast(move(this->solution));
    }

    bool attemptInsert(size_t rowIndex, const std::vector<double> &data) {
        if (this->solution->size() >= this->k) {
            return false;
        }

        double d_i = std::sqrt(getDotProduct(data, data));
        std::vector<double> c_i;

        for (size_t j = 0; j < this->solution->size(); j++) {
            const double e_i = (this->getDotProduct(data, *(solutionRows->at(j))) - this->getDotProduct(b->at(j), c_i)) / d->at(j);
            c_i.push_back(e_i);
            d_i = std::sqrt(std::pow(d_i, 2) - std::pow(e_i, 2));
        }

        const double marginal = std::log(std::pow(d_i, 2));
        if (this->passesThreshold(marginal)) {
            this->solution->addRow(rowIndex, marginal);
            this->solutionRows->push_back(&data);
            this->d->push_back(d_i);
            this->b->push_back(c_i);
            return true;
        } 

        return false;
    }

    private:
    bool passesThreshold(double marginalGain) {
        return marginalGain >= (((this->threshold / 2) - this->solution->getScore()) / (this->k - this->solution->size()));
    }

    static double getDotProduct(const std::vector<double> &a, const std::vector<double> &b) {
        double res = 0;
        for (size_t i = 0; i < a.size() && i < b.size(); i++) {
            res += a[i] * b[i];
        }
    
        return res;
    }
};