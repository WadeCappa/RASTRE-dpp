#include <Eigen/Dense>

#include <vector>
#include <memory>
#include <unordered_map>

class KernelMatrix {
    public:

    /** 
     * Needs to be parallel safe for row based indexes, otherwise race conditions will occur
     *  during getDiagonals()
     */
    virtual double get(size_t j, size_t i) = 0;

    virtual size_t size() = 0;

    double getCoverage() {
        return KernelMatrix::getCoverage(this->getDiagonals());
    }

    std::vector<double> getDiagonals() {
        const size_t s = this->size();
        std::vector<double> res(s);
        
        #pragma omp parallel for
        for (size_t index = 0; index < s; index++) {
            res[index] = this->get(index, index);
        }

        return move(res);
    }

    static double getDotProduct(const std::vector<double> &a, const std::vector<double> &b) {
        double res = 0;
        for (size_t i = 0; i < a.size() && i < b.size(); i++) {
            res += a[i] * b[i];
        }
    
        return res;
    }

    static double getCoverage(std::vector<double> diagonals) {
        double res = 0;
        for (size_t index = 0; index < diagonals.size(); index++) {
            res += std::log(diagonals[index]);
        }

        return res * 2;
    }
};

class LazyKernelMatrix : public KernelMatrix {
    private:
    const BaseData &data;

    std::vector<std::unordered_map<size_t, double>> kernelMatrix;
    std::unique_ptr<RelevanceCalculator> calc;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    LazyKernelMatrix(const LazyKernelMatrix &);

    public:
    static std::unique_ptr<LazyKernelMatrix> from(const BaseData &data) {
        return std::make_unique<LazyKernelMatrix>(data, std::unique_ptr<RelevanceCalculator>(new NaiveRelevanceCalculator(data)));
    }

    LazyKernelMatrix(const BaseData &data, std::unique_ptr<RelevanceCalculator> calc) 
    : 
        kernelMatrix(data.totalRows(), std::unordered_map<size_t, double>()),
        data(data),
        calc(move(calc))
    {}

    size_t size() {
        return this->data.totalRows();
    }

    double get(size_t j, size_t i) {
        const size_t forward_key = std::max(j, i);
        const size_t back_key = std::min(j, i);
        if (this->kernelMatrix[forward_key].find(back_key) == this->kernelMatrix[forward_key].end()) {
            double score = calc->get(forward_key, back_key);
            this->kernelMatrix[forward_key].insert({back_key, score});
        }

        return this->kernelMatrix[forward_key][back_key];
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
        return NaiveKernelMatrix::from(data, std::unique_ptr<RelevanceCalculator>(new NaiveRelevanceCalculator(data)));
    }

    static std::unique_ptr<NaiveKernelMatrix> from(
        const BaseData &data, 
        std::unique_ptr<RelevanceCalculator> calc) {
        std::vector<std::vector<double>> result(
            data.totalRows(), 
            std::vector<double>(data.totalRows(), 0)
        );

        // TODO: Verify that this is parallel safe, from a glance this looks super dangerous
        #pragma omp parallel for
        for (size_t i = 0; i < data.totalRows(); i++) {
            for (size_t j = i; j < data.totalRows(); j++) {
                double score = calc->get(i, j);
                result[j][i] = score;
                result[i][j] = score;
            }
        }

        return std::unique_ptr<NaiveKernelMatrix>(new NaiveKernelMatrix(move(result)));
    }

    NaiveKernelMatrix(std::vector<std::vector<double>> data) : kernelMatrix(move(data)) {}

    size_t size() {
        return this->kernelMatrix.size();
    }

    double get(size_t j, size_t i) {
        return this->kernelMatrix[j][i];
    }
};