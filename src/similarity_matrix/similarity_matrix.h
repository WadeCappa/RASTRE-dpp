#include <vector>
#include <cassert>
#include <cmath>
#include <Eigen/Dense>

#define assertm(exp, msg) assert(((void)msg, exp))

class SimilarityMatrix {
    private:
    std::vector<const std::vector<double>*> baseRows;

    double getSimilarityScore(const std::vector<double> v1, const std::vector<double> v2) const {
        double res = 0;
        assertm(v1.size() == v2.size(), "Expected input data rows to be the same size.");
        for (size_t index = 0; index < v1.size() && index < v2.size(); index++) {
            res += v1[index] * v2[index];
        }

        return res;
    }

    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> getMatrix() const {
        Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> res(this->baseRows.size(), this->baseRows.size());
        for (size_t j = 0; j < this->baseRows.size(); j++) {
            for (size_t i = 0; i < this->baseRows[j]->size(); i++) {
                const auto & v = this->baseRows[j]->at(i);
                res(j, i) = v;
            }
        }

        return res.transpose() * res;
    }

    public:
    SimilarityMatrix(const std::vector<double> &initialVector) {
        this->addRow(initialVector);
    }

    void addRow(const std::vector<double> &newRow) {
        this->baseRows.push_back(&newRow);
    }

    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> DEBUG_getMatrix() const {
        return getMatrix();
    }

    double getCoverage() const {
        auto matrix = getMatrix();
        return std::log10(matrix.determinant()) * 0.5;
    }
};