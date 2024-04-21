#include <Eigen/Dense>

#include <vector>
#include <unordered_map>

class KernelMatrix {
    public:
    virtual double get(size_t j, size_t i) = 0;
    virtual std::vector<double> getDiagonals(size_t rows) = 0;


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
    std::vector<std::unordered_map<size_t, double>> kernelMatrix;
    const BaseData &data;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    LazyKernelMatrix(const LazyKernelMatrix &);

    public:
    LazyKernelMatrix(const BaseData &data)
    : 
        kernelMatrix(data.totalRows(), std::unordered_map<size_t, double>()),
        data(data)
    {}

    double get(size_t j, size_t i) {
        if (this->kernelMatrix[j].find(i) == this->kernelMatrix[j].end()) {
            double result = this->data.getRow(j).dotProduct(this->data.getRow(i));
            result += static_cast<int>(j == i);
            this->kernelMatrix[j].insert({i, result});
            this->kernelMatrix[i].insert({j, result});
        }

        return this->kernelMatrix[j][i];
    }

    std::vector<double> getDiagonals(size_t rows) {
        std::vector<double> res(rows);
        
        #pragma omp parallel for
        for (size_t row = 0; row < rows; row++) {
            double result = this->data.getRow(row).dotProduct(this->data.getRow(row));
            result++; // add identity matrix
            res[row] = result;
            
            // Can be accessed in parallel since the vector's maps are only accessed exactly once and 
            //  the vector itself is never resized.
            this->kernelMatrix[row].insert({row, result});
        }

        return move(res);
    }
};

class NaiveKernelMatrix : public KernelMatrix {
    private:
    std::vector<std::vector<double>> kernelMatrix;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    NaiveKernelMatrix(const NaiveKernelMatrix &);

    public:
    static std::unique_ptr<NaiveKernelMatrix> from(const BaseData &data) {
        std::vector<std::vector<double>> result(
            data.totalRows(), 
            std::vector<double>(data.totalRows(), 0)
        );

        #pragma omp parallel for
        for (size_t i = 0; i < data.totalRows(); i++) {
            for (size_t j = i; j < data.totalRows(); j++) {
                double dotProduct = data.getRow(j).dotProduct(data.getRow(i));
                dotProduct += static_cast<int>(j == i);
                result[j][i] = dotProduct;
                result[i][j] = dotProduct;
            }
        }

        return std::unique_ptr<NaiveKernelMatrix>(new NaiveKernelMatrix(move(result)));
    }

    NaiveKernelMatrix(std::vector<std::vector<double>> data) : kernelMatrix(move(data)) {}

    double get(size_t j, size_t i) {
        return this->kernelMatrix[j][i];
    }

    std::vector<double> getDiagonals(size_t rows) {
        std::vector<double> res(rows);
        
        for (size_t index = 0; index < rows; index++) {
            res[index] = this->get(index, index);
        }

        return move(res);
    }
};