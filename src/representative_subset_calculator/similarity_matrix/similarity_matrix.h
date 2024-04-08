#include <vector>
#include <cassert>
#include <cmath>
#include <Eigen/Dense>

#define assertm(exp, msg) assert(((void)msg, exp))

class SimilarityMatrix {
    public:
    virtual double getCoverage() const = 0; 
};

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