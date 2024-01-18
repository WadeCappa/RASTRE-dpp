#include <vector>
#include <cassert>
#include <cmath>
#include <Eigen/Dense>

#define assertm(exp, msg) assert(((void)msg, exp))

class SimilarityMatrix {
    private:
    std::vector<const std::vector<double>*> baseRows;

    Eigen::MatrixXd getKernelMatrix() const {
        Eigen::MatrixXd matrix(this->baseRows.size(), this->baseRows[0]->size());
        for (int i = 0; i < this->baseRows.size(); i++) {
            matrix.row(i) = Eigen::VectorXd::Map(this->baseRows[i]->data(), this->baseRows[i]->size());
        }

        auto transpose = Eigen::MatrixXd(matrix * matrix.transpose());
        for (size_t index = 0; index < transpose.rows(); index++) {
            transpose(index,index) += 1;
        }
        return transpose;
    }

    public:
    SimilarityMatrix() {}

    SimilarityMatrix(const std::vector<double> &initialVector) {
        this->addRow(initialVector);
    }

    SimilarityMatrix(const SimilarityMatrix& t) : baseRows(t.getBase()) {}

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
        auto kernelMatrix = getKernelMatrix();
        Eigen::MatrixXd diagonal(kernelMatrix.llt().matrixL());

        double res = 0;
        for (size_t index = 0; index < diagonal.rows(); index++) {
            res += std::log(diagonal(index,index));
        }

        // std::cout << "determinant: " << std::log(kernelMatrix.determinant()) << ", cholskey determanant: " << res * 2 << std::endl;
        return res * 2;
    }
};