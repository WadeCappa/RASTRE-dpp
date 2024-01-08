#include <vector>
#include <cassert>
#include <cmath>
#include <Eigen/Dense>

#define assertm(exp, msg) assert(((void)msg, exp))

class SimilarityMatrix {
    private:
    std::vector<const std::vector<double>*> baseRows;
    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> similarity;

    double getSimilarityScore(const std::vector<double> v1, const std::vector<double> v2) const {
        double res = 0;
        assertm(v1.size() == v2.size(), "Expected input data rows to be the same size.");
        for (size_t index = 0; index < v1.size() && index < v2.size(); index++) {
            res += v1[index] * v2[index];
        }

        return res;
    }

    public:
    SimilarityMatrix(const std::vector<double> &initialVector) : similarity() {
        this->addRow(initialVector);
    }

    void addRow(const std::vector<double> &newRow) {
        size_t newSize = this->similarity.size() + 1;
        this->similarity.resize(newSize, newSize);
        for (size_t index = 0; index < this->baseRows.size(); index++) {
            this->similarity(index, newSize - 1) = this->getSimilarityScore(*this->baseRows[index], newRow);
        }
        
        this->baseRows.push_back(&newRow);

        for (size_t index = 0; index < this->baseRows.size(); index++) {
            this->similarity(newSize - 1, index) = this->getSimilarityScore(newRow, *this->baseRows[index]);
        }
    }

    double getCoverage() const {
        return std::log10(this->similarity.determinant()) * 0.5;
    }
};