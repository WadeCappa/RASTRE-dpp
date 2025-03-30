#include <vector>
#include <cassert>
#include <cmath>
#include <Eigen/Dense>

#define assertm(exp, msg) assert(((void)msg, exp))

class SimilarityMatrix {
    public:
    virtual ~SimilarityMatrix() {}
    virtual float getCoverage() const = 0; 
};

class MutableSimilarityMatrix : public SimilarityMatrix {
    private:
    std::vector<const DataRow*> baseRows;
    std::vector<std::vector<float>> transposeMatrix;

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

    const std::vector<std::vector<float>> &getTranspose() const {
        return this->transposeMatrix;
    }

    void addRow(const DataRow &newRow) {
        this->transposeMatrix.push_back(std::vector<float>(this->transposeMatrix.size() + 1));
        for (size_t i = 0; i < this->baseRows.size(); i++) {
            this->transposeMatrix[i].push_back(this->baseRows[i]->dotProduct(newRow));
            this->transposeMatrix.back()[i] = this->transposeMatrix[i].back();
        }

        baseRows.push_back(&newRow);
        this->transposeMatrix.back().back() = newRow.dotProduct(newRow) + 1;
    }

    float getCoverage() const {
        Eigen::MatrixXf kernelMatrix(this->transposeMatrix.size(), this->transposeMatrix.size());
        for (int i = 0; i < this->transposeMatrix.size(); i++) {
            kernelMatrix.row(i) = Eigen::VectorXf::Map(this->transposeMatrix[i].data(), this->transposeMatrix[i].size());
        }

        Eigen::MatrixXf diagonal(kernelMatrix.llt().matrixL());

        float res = 0;
        for (size_t index = 0; index < diagonal.rows(); index++) {
            res += diagonal(index,index);
        }

        return res * 2;
    }
};