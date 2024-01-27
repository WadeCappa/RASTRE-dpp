#include <Eigen/Dense>

#include <vector>
#include <optional>
#include <optional>

class KernelMatrix {
    public:
    virtual double get(size_t j, size_t i) = 0;

    std::vector<double> getDiagonals(size_t rows) {
        std::vector<double> res(rows);
        for (size_t index = 0; index < rows; index++) {
            res[index] = this->get(index, index);
        }

        return res;
    }

    static double getDotProduct(const std::vector<double> &a, const std::vector<double> &b) {
        double res = 0;
        for (size_t i = 0; i < a.size() && i < b.size(); i++) {
            res += a[i] * b[i];
        }
    
        return res;
    }
};

class LazyKernelMatrix : public KernelMatrix {
    private:
    const Data &data;
    std::vector<std::vector<std::optional<double>>> kernelMatrix;

    public:
    LazyKernelMatrix(const Data &data) : data(data), kernelMatrix(data.rows, std::vector<std::optional<double>>(data.rows, std::nullopt)) {}

    double get(size_t j, size_t i) {
        if (!this->kernelMatrix[j][i].has_value()) {
            double dotProduct = KernelMatrix::getDotProduct(this->data.data[j], this->data.data[i]);
            dotProduct += static_cast<int>(j == i);
            this->kernelMatrix[j][i] = dotProduct;
            this->kernelMatrix[i][j] = dotProduct;
        }

        return this->kernelMatrix[j][i].value();
    }
};

class NaiveKernelMatrix : public KernelMatrix {
    private:
    Eigen::MatrixXd kernelMatrix;

    Eigen::MatrixXd buildKernelMatrix(const Data &data) {
        Eigen::MatrixXd rawDataMatrix(data.rows, data.columns);
        for (int i = 0; i < data.rows; i++) {
            rawDataMatrix.row(i) = Eigen::VectorXd::Map(data.data[i].data(), data.columns);
        }

        return Eigen::MatrixXd(rawDataMatrix * rawDataMatrix.transpose());
    }

    public:
    NaiveKernelMatrix(const Data &data) : kernelMatrix(buildKernelMatrix(data)) {
        for (size_t index = 0; index < kernelMatrix.rows(); index++) {
            // add identity matrix
            kernelMatrix(index, index) += 1;
        }
    }

    double get(size_t j, size_t i) {
        return this->kernelMatrix(j, i);
    }
};