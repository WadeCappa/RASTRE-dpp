#include <Eigen/Dense>
#include <vector>
#include <math.h>
#include <unordered_set>
#include <optional>

class LazyFastSubsetCalculator : public SubsetCalculator {
    private:
    const double epsilon;

    struct HeapComparitor {
        const std::vector<double> &diagonals;
        HeapComparitor(const std::vector<double> &diagonals) : diagonals(diagonals) {}
        bool operator()(size_t a, size_t b) {
            return std::log(diagonals[a]) < std::log(diagonals[b]);
        }
    };

    static std::vector<double> getSlice(const std::vector<double> &row, const MutableSubset* solution, size_t count) {
        std::vector<double> res(count);
        for (size_t i = 0; i < count ; i++) {
            res[i] = row[solution->getRow(i)];
        }

        return res;
    }

    public:
    LazyFastSubsetCalculator(const double epsilon) : epsilon(epsilon) {
        if (this->epsilon < 0) {
            throw std::invalid_argument("Epsilon is less than 0.");
        }
    }

    // TODO: Break when marginal gain is below epsilon
    std::unique_ptr<Subset> getApproximationSet(const Data &data, size_t k) {
        MutableSubset* solution = new MutableSubset();
        std::unordered_set<size_t> seen;
        std::vector<std::vector<double>> v(data.totalRows(), std::vector<double>(data.totalRows()));
        std::vector<size_t> u(data.totalRows(), 0);

        // Initialize kernel matrix 
        LazyKernelMatrix kernelMatrix(data);
        std::vector<double> diagonals = kernelMatrix.getDiagonals(data.totalRows());

        // Initialize priority queue
        std::vector<size_t> priorityQueue;
        for (size_t index = 0; index < data.totalRows(); index++) {
            priorityQueue.push_back(index);
        }

        HeapComparitor comparitor(diagonals);
        std::make_heap(priorityQueue.begin(), priorityQueue.end(), comparitor);

        while (solution->size() < k) {
            size_t i = priorityQueue.front();
            std::pop_heap(priorityQueue.begin(),priorityQueue.end(), comparitor); 
            priorityQueue.pop_back();

            // update row
            for (size_t t = u[i]; t < solution->size(); t++) {
                size_t j_t = solution->getRow(t); 
                double dotProduct = KernelMatrix::getDotProduct(this->getSlice(v[i], solution, t), this->getSlice(v[j_t], solution, t));
                v[i][j_t] = (kernelMatrix.get(i, j_t) - dotProduct) / std::sqrt(diagonals[j_t]);
                diagonals[i] -= std::pow(v[i][j_t], 2);
            }

            u[i] = solution->size();
            
            double marginalGain = std::log(diagonals[i]);
            double nextScore = std::log(diagonals[priorityQueue.front()]);

            if (marginalGain > nextScore || solution->size() == data.totalRows() - 1) {
                solution->addRow(i, marginalGain);
            } else {
                priorityQueue.push_back(i);
                std::push_heap(priorityQueue.begin(), priorityQueue.end(), comparitor);
            }
        }

        return MutableSubset::upcast(solution);
    }
};