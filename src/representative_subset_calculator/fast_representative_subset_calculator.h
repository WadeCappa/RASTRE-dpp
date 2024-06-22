#include <Eigen/Dense>
#include <vector>
#include <math.h>
#include <unordered_set>

class FastSubsetCalculator : public SubsetCalculator {
    private:
    const double epsilon;

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
        
        return std::make_pair(bestRow, highestScore);
    }

  public:
    FastSubsetCalculator(const double epsilon) : epsilon(epsilon) {
        if (this->epsilon < 0) {
            throw std::invalid_argument("Epsilon is less than 0.");
        }
    }

    std::unique_ptr<Subset> getApproximationSet(
        std::unique_ptr<MutableSubset> consumer, 
        const BaseData &data, 
        size_t k
    ) {
        std::unordered_set<size_t> seen;

        std::unique_ptr<NaiveKernelMatrix> kernelMatrix(NaiveKernelMatrix::from(data));
        std::vector<double> diagonals = kernelMatrix->getDiagonals(); 

        std::vector<std::vector<double>> c(data.totalRows(), std::vector<double>());

        auto bestScore = getNextHighestScore(diagonals, seen);
        size_t j = bestScore.first;
        seen.insert(j);
        consumer->addRow(j, bestScore.second);

        while (consumer->size() < k) {
            #pragma omp parallel for 
            for (size_t i = 0; i < data.totalRows(); i++) {
                if (seen.find(i) != seen.end()) {
                    continue;
                }
                
                double e = (kernelMatrix->get(j, i) - KernelMatrix::getDotProduct(c[j], c[i])) / std::sqrt(diagonals[j]);
                c[i].push_back(e);
                diagonals[i] -= std::pow(e, 2);
            }

            bestScore = getNextHighestScore(diagonals, seen);

            // To ensure "Numerical stability" ¯\_(ツ)_/¯
            if (bestScore.second <= this->epsilon) {
                std::cout << "score of " << bestScore.second << " was less than " << this->epsilon << ". " << std::endl;
                return MutableSubset::upcast(move(consumer));
            }

            j = bestScore.first;
            seen.insert(j);
            consumer->addRow(j, bestScore.second);
        }
    
        return MutableSubset::upcast(move(consumer));
    }
};