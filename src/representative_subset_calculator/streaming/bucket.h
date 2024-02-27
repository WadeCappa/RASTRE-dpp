#include <vector>
#include <memory>

class ThresholdBucket 
{
    private:
    std::unique_ptr<MutableSubset> solution;
    double marginalGainThreshold;
    int k;
    MutableSimilarityMatrix matrix;

    public:
    ThresholdBucket(const double threshold, const int k) : marginalGainThreshold(threshold), k(k), solution(NaiveMutableSubset::makeNew()) {}

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

        MutableSimilarityMatrix tempMatrix(matrix);
        tempMatrix.addRow(data);
        double marginal = tempMatrix.getCoverage() - solution->getScore();

        if (marginal >= this-> marginalGainThreshold) {
            this->solution->addRow(rowIndex, marginal);
            this->matrix.addRow(data);

            return true;
        }

        return false;
    }
};