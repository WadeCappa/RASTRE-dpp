#include <Eigen/Dense>
#include <vector>
#include <math.h>
#include <unordered_set>
#include <optional>
#include "kernel_matrix/kernel_matrix.h"

class LazyFastRepresentativeSubsetCalculator : public RepresentativeSubsetCalculator {
    private:
    Timers &timers;
    double epsilon;

    struct HeapComparitor {
        const std::vector<double> &diagonals;
        HeapComparitor(const std::vector<double> &diagonals) : diagonals(diagonals) {}
        bool operator()(size_t a, size_t b) {
            return diagonals[a] < diagonals[b];
        }
    };

    static std::vector<double> getSlice(const std::vector<double> &row, const std::vector<size_t> solution, size_t count) {
        std::vector<double> res(count);
        for (size_t i = 0; i < count ; i++) {
            res[i] = row[solution[i]];
        }

        return res;
    }

    public:
    LazyFastRepresentativeSubsetCalculator(Timers &timers, const double epsilon) : timers(timers), epsilon(epsilon) {
        if (this->epsilon < 0) {
            throw std::invalid_argument("Epsilon is less than 0.");
        }
    }

    RepresentativeSubset getApproximationSet(const Data &data, size_t k) {
        timers.totalCalculationTime.startTimer();

        double totalScore = 0;
        std::vector<size_t> solution;
        std::unordered_set<size_t> seen;
        std::vector<std::vector<double>> v(data.rows, std::vector<double>(data.rows));
        std::vector<size_t> u(data.rows, 0);

        // Initialize kernel matrix 
        LazyKernelMatrix kernelMatrix(data);

        // Initialize priority queue
        std::vector<size_t> priorityQueue;
        std::vector<double> diagonals(data.rows);
        for (size_t index = 0; index < data.rows; index++) {
            diagonals[index] = std::sqrt(kernelMatrix.get(index, index));
            priorityQueue.push_back(index);
        }

        HeapComparitor comparitor(diagonals);
        std::make_heap(priorityQueue.begin(), priorityQueue.end(), comparitor);

        while (solution.size() < k) {
            size_t i = priorityQueue.front();
            std::pop_heap(priorityQueue.begin(),priorityQueue.end(), comparitor); 
            priorityQueue.pop_back();

            // update row
            for (size_t t = u[i]; t < solution.size(); t++) {
                size_t j_t = solution[t]; 
                double dotProduct = KernelMatrix::getDotProduct(this->getSlice(v[i], solution, t), this->getSlice(v[j_t], solution, t));
                v[i][j_t] = (kernelMatrix.get(i, j_t) - dotProduct) / diagonals[j_t];
                diagonals[i] = std::sqrt(std::pow(diagonals[i], 2) - std::pow(v[i][j_t], 2));
            }

            u[i] = solution.size();

            double nextScore = diagonals[priorityQueue.front()];
            if (diagonals[i] > nextScore || solution.size() == data.rows - 1) {
                solution.push_back(i);
                std::cout << "lazy fast found " << i << " which increasd marginal score by " << diagonals[i] << std::endl;
                totalScore += diagonals[i];
            } else {
                priorityQueue.push_back(i);
                std::push_heap(priorityQueue.begin(), priorityQueue.end(), comparitor);
            }
        }

        timers.totalCalculationTime.stopTimer();

        RepresentativeSubset subset;
        subset.representativeRows = solution;
        subset.coverage = totalScore;
        return subset;
    }
};