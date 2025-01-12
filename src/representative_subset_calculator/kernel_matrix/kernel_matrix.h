#include <Eigen/Dense>

#include <vector>
#include <memory>
#include <unordered_map>

class KernelMatrix {
    public:
    virtual ~KernelMatrix() {}

    /** 
     * Needs to be parallel safe for row based indexes, otherwise race conditions will occur
     *  during getDiagonals()
     */
    virtual float get(size_t j, size_t i) = 0;

    virtual size_t size() = 0;

    float getCoverage() {
        return KernelMatrix::getCoverage(this->getDiagonals());
    }

    std::vector<float> getDiagonals() {
        const size_t s = this->size();
        std::vector<float> res(s);
        
        #pragma omp parallel for
        for (size_t index = 0; index < s; index++) {
            res[index] = this->get(index, index);
        }

        return move(res);
    }

    static float getDotProduct(const std::vector<float> &a, const std::vector<float> &b) {
        float res = 0;
        for (size_t i = 0; i < a.size() && i < b.size(); i++) {
            res += a[i] * b[i];
        }
        return res;
    }

    static float getCoverage(std::vector<float> diagonals) {
        float res = 0;
        for (size_t index = 0; index < diagonals.size(); index++) {
            res += std::log(diagonals[index]);
        }

        return res * 2;
    }
};

class LazyKernelMatrix : public KernelMatrix {
    private:
    const BaseData &data;

    std::vector<std::unordered_map<size_t, float>> kernelMatrix;
    RelevanceCalculator& calc;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.

    // L[i][j] = r[i] * S[i][j] * r[j] <- for user mode
    LazyKernelMatrix(const LazyKernelMatrix &);

    public:
    static std::unique_ptr<LazyKernelMatrix> from(
        const BaseData &data, 
        RelevanceCalculator& calc) {
        return std::make_unique<LazyKernelMatrix>(data, calc);
    }

    LazyKernelMatrix(const BaseData &data, RelevanceCalculator& calc) 
    : 
        kernelMatrix(data.totalRows(), std::unordered_map<size_t, float>()),
        data(data),
        calc(calc)
    {}

    size_t size() {
        return this->data.totalRows();
    }

    // This will also be affected during user mode. This means we should build the kernel matrix 
    // with the user specific information
    float get(size_t j, size_t i) {
        const size_t forward_key = std::max(j, i);
        const size_t back_key = std::min(j, i);
        if (this->kernelMatrix[forward_key].find(back_key) == this->kernelMatrix[forward_key].end()) {
            float score = calc.get(forward_key, back_key);
            this->kernelMatrix[forward_key].insert({back_key, score});
        }

        return this->kernelMatrix[forward_key][back_key];
    }
};

class NaiveKernelMatrix : public KernelMatrix {
    private:
    std::vector<std::vector<float>> kernelMatrix;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    NaiveKernelMatrix(const NaiveKernelMatrix &);

    public:
    static std::unique_ptr<NaiveKernelMatrix> from(
        const BaseData &data, 
        RelevanceCalculator &calc) {
        std::vector<std::vector<float>> result(
            data.totalRows(), 
            std::vector<float>(data.totalRows(), 0)
        );

        // TODO: Verify that this is parallel safe, from a glance this looks super dangerous
        #pragma omp parallel for
        for (size_t i = 0; i < data.totalRows(); i++) {
            for (size_t j = i; j < data.totalRows(); j++) {
                float score = calc.get(i, j);
                result[j][i] = score;
                result[i][j] = score;
            }
        }

        return std::unique_ptr<NaiveKernelMatrix>(new NaiveKernelMatrix(move(result)));
    }

    NaiveKernelMatrix(std::vector<std::vector<float>> data) : kernelMatrix(move(data)) {}

    size_t size() {
        return this->kernelMatrix.size();
    }

    float get(size_t j, size_t i) {
        return this->kernelMatrix[j][i];
    }
};