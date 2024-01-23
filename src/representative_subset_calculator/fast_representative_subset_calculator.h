#include <Eigen/Dense>
#include <vector>
#include <math.h>
#include <unordered_set>

class FastRepresentativeSubsetCalculator : public RepresentativeSubsetCalculator {
    private:
    Timers &timers;
    double epsilon;

    static Eigen::MatrixXd getKernelMatrix(const Data &data) {
        Eigen::MatrixXd matrix(data.rows, data.columns);
        for (int i = 0; i < data.rows; i++) {
            matrix.row(i) = Eigen::VectorXd::Map(data.data[i].data(), data.columns);
        }

        auto kernelMatrix = Eigen::MatrixXd(matrix * matrix.transpose());
        for (size_t index = 0; index < kernelMatrix.rows(); index++) {
            // add identity matrix
            kernelMatrix(index, index) += 1;
        }

        return kernelMatrix;
    }

    static std::vector<double> getDiagonalVector(const Eigen::MatrixXd &kernelMatrix) {
        std::vector<double> diagonals(kernelMatrix.rows());
        for (size_t i = 0; i < kernelMatrix.rows(); i++) {
            diagonals[i] = kernelMatrix(i, i);
        }

        return diagonals;
    }

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

    static double getDotProduct(const std::vector<double> &a, const std::vector<double> &b) {
        double res = 0;
        for (size_t i = 0; i < a.size() && i < b.size(); i++) {
            res += a[i] * b[i];
        }
    
        return res;
    }

  public:
    FastRepresentativeSubsetCalculator(Timers &timers, const double epsilon) : timers(timers), epsilon(epsilon) {}

    RepresentativeSubset getApproximationSet(const Data &data, size_t k) {
        timers.totalCalculationTime.startTimer();

        std::vector<size_t> solution;
        std::unordered_set<size_t> seen;

        Eigen::MatrixXd kernelMatrix = this->getKernelMatrix(data);
        std::vector<double> diagonals = this->getDiagonalVector(kernelMatrix); 

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
                
                double e = (kernelMatrix(j, i) - getDotProduct(c[j], c[i])) / std::sqrt(diagonals[j]);
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