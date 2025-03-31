
#include <vector>
#include <memory>
#include <unordered_map>

#include "relevance_calculator.h"
#include "../../data_tools/base_data.h"

#ifndef KERNEL_MATRIX_H
#define KERNEL_MATRIX_H

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

        return std::move(res);
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
            res += diagonals[index];
        }

        return res * 2;
    }
};

class LazyKernelMatrix : public KernelMatrix {
    public:
    inline static size_t getRowKey(const size_t j, const size_t i) {
        return std::max(j, i);
    }
    inline static size_t getColumnKey(const size_t j, const size_t i) {
        return std::min(j, i);
    }
};

class UnsafeLazyKernelMatrix : public LazyKernelMatrix {
    private:
    const BaseData &data;

    std::vector<std::unordered_map<size_t, float>> kernelMatrix;
    RelevanceCalculator& calc;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.

    // L[i][j] = r[i] * S[i][j] * r[j] <- for user mode
    UnsafeLazyKernelMatrix(const UnsafeLazyKernelMatrix &);

    public:
    static std::unique_ptr<UnsafeLazyKernelMatrix> from(
        const BaseData &data, 
        RelevanceCalculator& calc) {
        return std::make_unique<UnsafeLazyKernelMatrix>(data, calc);
    }

    UnsafeLazyKernelMatrix(const BaseData &data, RelevanceCalculator& calc) 
    : 
        kernelMatrix(data.totalRows(), std::unordered_map<size_t, float>()),
        data(data),
        calc(calc)
    {}

    size_t size() {
        return this->data.totalRows();
    }

    float get(size_t j, size_t i) {
        const size_t row_key = getRowKey(j, i);
        const size_t column_key = getColumnKey(j, i);
        if (this->kernelMatrix[row_key].find(column_key) == this->kernelMatrix[row_key].end()) {
            float score = calc.get(row_key, column_key);
            this->kernelMatrix[row_key].insert({column_key, score});
        }

        return this->kernelMatrix[row_key][column_key];
    }
};

class ThreadSafeLazyKernelMatrix : public LazyKernelMatrix {
    private:
    std::unique_ptr<KernelMatrix> delegate;
    std::vector<std::mutex> rowLocks;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    ThreadSafeLazyKernelMatrix(const ThreadSafeLazyKernelMatrix &);

    public:
    static std::unique_ptr<ThreadSafeLazyKernelMatrix> from(
        const BaseData &data, 
        RelevanceCalculator& calc) {
        std::unique_ptr<UnsafeLazyKernelMatrix> delegate(UnsafeLazyKernelMatrix::from(data, calc));
        std::vector<std::mutex> rowLocks(delegate->size());
        return std::make_unique<ThreadSafeLazyKernelMatrix>(
            std::move(delegate), std::move(rowLocks)
        );
    }

    ThreadSafeLazyKernelMatrix(
        std::unique_ptr<KernelMatrix> delegate, 
        std::vector<std::mutex> rowLocks
    ) : 
        delegate(move(delegate)),
        rowLocks(move(rowLocks))
    {}

    size_t size() {
        return this->delegate->size();
    }

    float get(size_t j, size_t i) {
        const size_t row_key = getRowKey(j, i);
        this->rowLocks[row_key].lock();
        const float result = this->delegate->get(j, i);
        this->rowLocks[row_key].unlock();
        return result;
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
                const float score = calc.get(i, j);
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

    void printDEBUG() const {
        for (const auto & r : kernelMatrix) {
            for (const auto & v : r) {
                std::cout << v << " ";
            }
            std::cout << std::endl;
        }
    }
};

#endif