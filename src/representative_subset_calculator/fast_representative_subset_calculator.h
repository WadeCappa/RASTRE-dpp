#include <Eigen/Dense>
#include <vector>
#include <math.h>
#include <unordered_set>
#include "kernel_matrix/kernel_matrix.h"

class FastRepresentativeSubsetCalculator : public RepresentativeSubsetCalculator {
    private:
    Timers &timers;
    double epsilon;

    static std::pair<size_t, double> getNextHighestScore(
        const std::vector<double> &diagonals, 
        const std::unordered_set<size_t> &seen 
    ) {
        size_t bestRow = -1;
        double highestScore = -1;

        for (size_t i = 0; i < diagonals.size(); i++) {
            double score = std::log(diagonals[i]);
            if (seen.find(i) == seen.end() && score > highestScore) {
                highestScore = score;
                bestRow = i;
            }
        }

        if (highestScore < 0) {
            std::cout << "failed to find next highest score" << std::endl;
        }
        
        std::cout << "FAST found " << bestRow << " which increased marginal by " << highestScore << std::endl;
        return std::make_pair(bestRow, highestScore);
    }

  public:
    FastRepresentativeSubsetCalculator(Timers &timers, const double epsilon) : timers(timers), epsilon(epsilon) {
        if (this->epsilon < 0) {
            throw std::invalid_argument("Epsilon is less than 0.");
        }
    }

    RepresentativeSubset getApproximationSet(const Data &data, size_t k) {
        timers.totalCalculationTime.startTimer();

        std::vector<size_t> solution;
        std::unordered_set<size_t> seen;

        NaiveKernelMatrix kernelMatrix(data);
        std::vector<double> diagonals = kernelMatrix.getDiagonals(data.rows); 

        std::vector<std::vector<double>> c(data.rows, std::vector<double>());

        // Modifies the solution set
        auto bestScore = getNextHighestScore(diagonals, seen);
        size_t j = bestScore.first;
        seen.insert(j);
        solution.push_back(j);
        double totalScore = bestScore.second;

        while (solution.size() < k) {
            #pragma omp parallel for 
            for (size_t i = 0; i < data.rows; i++) {
                if (seen.find(i) != seen.end()) {
                    continue;
                }
                
                double e = (kernelMatrix.get(j, i) - KernelMatrix::getDotProduct(c[j], c[i])) / std::sqrt(diagonals[j]);
                c[i].push_back(e);
                diagonals[i] -= std::pow(e, 2);
            }

            // Modifies the solution set
            bestScore = getNextHighestScore(diagonals, seen);
            if (bestScore.second <= this->epsilon) {
                std::cout << "score of " << bestScore.second << " was less than " << this->epsilon << ". " << std::endl;
                timers.totalCalculationTime.stopTimer();
                RepresentativeSubset subset;
                subset.representativeRows = solution;
                subset.coverage = totalScore;
                return subset;
            }

            j = bestScore.first;
            seen.insert(j);
            solution.push_back(j);
            totalScore += bestScore.second;
        }
    
        timers.totalCalculationTime.stopTimer();
        RepresentativeSubset subset;
        subset.representativeRows = solution;
        subset.coverage = totalScore;
        return subset;
    }
};