#include <vector>
#include <cassert>
#include <cmath>
#include <Eigen/Dense>

#define assertm(exp, msg) assert(((void)msg, exp))

class SimilarityMatrix {
    public:
    virtual double getCoverage() const = 0; 
};

// // This class will be faster at calculating the score than the MutableSimilarityMatrix iff 
// //  all rows are known when this matrix is being built.
// class ImmutableSimilarityMatrix : public SimilarityMatrix {
//     private:
//     std::vector<const DataRow*> baseRows;

//     Eigen::MatrixXd getKernelMatrix() const {
//         Eigen::MatrixXd matrix(this->baseRows.size(), this->baseRows[0]->size());
//         for (int i = 0; i < this->baseRows.size(); i++) {
//             matrix.row(i) = Eigen::VectorXd::Map(this->baseRows[i]->data(), this->baseRows[i]->size());
//         }

//         auto transpose = Eigen::MatrixXd(matrix * matrix.transpose());
//         for (size_t index = 0; index < transpose.rows(); index++) {
//             transpose(index,index) += 1;
//         }
//         return transpose;
//     }

//     // Should ideally not be copied
//     ImmutableSimilarityMatrix(const ImmutableSimilarityMatrix&);

//     public:
//     ImmutableSimilarityMatrix(const std::vector<std::vector<double>> &data) {
//         for (const auto & v : data) {
//             this->baseRows.push_back(&v);
//         }
//     }

//     ImmutableSimilarityMatrix(const std::vector<double> &initialVector) {
//         this->baseRows.push_back(&initialVector);
//     }

//     double getCoverage() const {
//         auto kernelMatrix = getKernelMatrix();
//         Eigen::MatrixXd diagonal(kernelMatrix.llt().matrixL());

//         double res = 0;
//         for (size_t index = 0; index < diagonal.rows(); index++) {
//             res += std::log(diagonal(index,index));
//         }

//         return res * 2;
//     }
// };

class MutableSimilarityMatrix : public SimilarityMatrix {
    private:
    std::vector<const DataRow*> baseRows;
    std::vector<std::vector<double>> transposeMatrix;

    public:
    MutableSimilarityMatrix() {}

    MutableSimilarityMatrix(const DataRow& firstRow) {
        this->addRow(firstRow);
    }

    MutableSimilarityMatrix(const MutableSimilarityMatrix& t) 
    : 
        baseRows(t.getBase()), 
        transposeMatrix(t.getTranspose()) 
    {}
    
    const std::vector<const DataRow*> &getBase() const {
        return this->baseRows;
    }

    const std::vector<std::vector<double>> &getTranspose() const {
        return this->transposeMatrix;
    }

    void addRow(const DataRow &newRow) {
        this->transposeMatrix.push_back(std::vector<double>(this->transposeMatrix.size() + 1));
        for (size_t i = 0; i < this->baseRows.size(); i++) {
            this->transposeMatrix[i].push_back(this->baseRows[i]->dotProduct(newRow));
            this->transposeMatrix.back()[i] = this->transposeMatrix[i].back();
        }

        baseRows.push_back(&newRow);
        this->transposeMatrix.back().back() = newRow.dotProduct(newRow) + 1;
    }

    double getCoverage() const {
        Eigen::MatrixXd kernelMatrix(this->transposeMatrix.size(), this->transposeMatrix.size());
        for (int i = 0; i < this->transposeMatrix.size(); i++) {
            kernelMatrix.row(i) = Eigen::VectorXd::Map(this->transposeMatrix[i].data(), this->transposeMatrix[i].size());
        }

        Eigen::MatrixXd diagonal(kernelMatrix.llt().matrixL());

        double res = 0;
        for (size_t index = 0; index < diagonal.rows(); index++) {
            res += std::log(diagonal(index,index));
        }

        return res * 2;
    }
};