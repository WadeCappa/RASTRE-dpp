#include <vector>
#include <cassert>
#include <cmath>
#include <Eigen/Dense>

#define assertm(exp, msg) assert(((void)msg, exp))

class SimilarityMatrix {
    private:
    std::vector<const std::vector<double>*> baseRows;

    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> getKernelMatrix() const {
        Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> res(this->baseRows.size(), this->baseRows[0]->size());
        for (size_t j = 0; j < this->baseRows.size(); j++) {
            for (size_t i = 0; i < this->baseRows[j]->size(); i++) {
                const auto & v = this->baseRows[j]->at(i);
                res(j, i) = v;
            }
        }

        return res * res.transpose();
    }

    public:
    SimilarityMatrix() {}

    SimilarityMatrix(const std::vector<double> &initialVector) {
        this->addRow(initialVector);
    }

    SimilarityMatrix(const SimilarityMatrix& t) : baseRows(t.getBase()) {

    }

    void addRow(const std::vector<double> &newRow) {
        this->baseRows.push_back(&newRow);
    }

    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> DEBUG_getMatrix() const {
        return getKernelMatrix();
    }

    std::vector<const std::vector<double>*> getBase() const {
        return this->baseRows;
    }

    double getCoverage() const {
        auto matrix = getKernelMatrix();
        return std::log(matrix.llt().matrixL().determinant()) * 0.5;
    }
};