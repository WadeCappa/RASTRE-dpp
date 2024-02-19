#include <vector>
#include <memory>

class ThresholdBucket 
{
    private:
    std::unique_ptr<MutableSubset> solution;
    double marginalGainThreshold;
    int k;
    SimilarityMatrix matrix;

    public:
    ThresholdBucket(const double threshold, const int k) : marginalGainThreshold(threshold), k(k), solution(NaiveMutableSubset::makeNew()) {}

    ThresholdBucket(size_t deltaZero, int k, double epsilon, size_t numBucket) 
    : 
        solution(NaiveMutableSubset::makeNew()), 
        marginalGainThreshold(((double)deltaZero / (double)( 2 * k )) * (double)std::pow(1 + epsilon, numBucket)),
        k(k)
    {}

    size_t getUtility() {
        return this->solution->getScore();
    }

    std::unique_ptr<Subset> returnSolutionDestroyBucket() {
        return MutableSubset::upcast(move(this->solution));
    }

    bool attemptInsert(size_t rowIndex, std::vector<double> data) {
        if (this->solution->size() >= this->k) {
            return false;
        }

        SimilarityMatrix tempMatrix(matrix);
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