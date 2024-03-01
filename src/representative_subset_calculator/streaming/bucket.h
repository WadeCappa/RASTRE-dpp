#include <vector>
#include <memory>

class ThresholdBucket 
{
    private:
    std::unique_ptr<MutableSubset> solution;
    double threshold;
    int k;
    std::unique_ptr<MutableSimilarityMatrix> matrix;

    public:
    ThresholdBucket(const double threshold, const int k) 
    : 
        threshold(threshold), 
        k(k), 
        solution(NaiveMutableSubset::makeNew()),
        matrix(std::unique_ptr<MutableSimilarityMatrix>(new MutableSimilarityMatrix()))
    {}

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

        std::unique_ptr<MutableSimilarityMatrix> tempMatrix(new MutableSimilarityMatrix(*matrix.get()));
        tempMatrix->addRow(data);
        double marginal = tempMatrix->getCoverage() - solution->getScore();

        if (this->passesThreshold(marginal)) {
            this->solution->addRow(rowIndex, marginal);
            this->matrix = move(tempMatrix);

            return true;
        } 

        return false;
    }

    private:
    bool passesThreshold(double marginalGain) {
        return marginalGain >= (((this->threshold / 2) - this->solution->getScore()) / (this->k - this->solution->size()));
    }
};