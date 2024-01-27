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
    std::vector<Eigen::VectorXd> data;
    std::vector<std::vector<std::optional<double>>> kernelMatrix;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    LazyKernelMatrix(const LazyKernelMatrix &);

    public:
    LazyKernelMatrix(const Data &data) : kernelMatrix(data.rows, std::vector<std::optional<double>>(data.rows, std::nullopt)) {
        for (const auto & row : data.data) {
            this->data.push_back(Eigen::VectorXd::Map(row.data(), data.columns));
        }
    }

    double get(size_t j, size_t i) {
        if (!this->kernelMatrix[j][i].has_value()) {
            double dotProduct = this->data[j].dot(this->data[i]);
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

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    NaiveKernelMatrix(const NaiveKernelMatrix &);

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