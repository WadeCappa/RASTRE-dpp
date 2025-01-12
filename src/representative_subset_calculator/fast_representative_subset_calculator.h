#include <Eigen/Dense>
#include <vector>
#include <math.h>
#include <unordered_set>

class FastSubsetCalculator : public SubsetCalculator {
    private:
    const float epsilon;

    static std::pair<size_t, float> getNextHighestScore(
        const std::vector<float> &diagonals, 
        const std::unordered_set<size_t> &seen 
    ) {
        size_t bestRow = -1;
        float highestScore = -1;

        for (size_t i = 0; i < diagonals.size(); i++) {
            float score = std::log(diagonals[i]);
            if (seen.find(i) == seen.end() && score > highestScore) {
                highestScore = score;
                bestRow = i;
            }
        }

        if (highestScore < 0) {
            spdlog::error("failed to find next highest score");
        }
        
        return std::make_pair(bestRow, highestScore);
    }

  public:
    FastSubsetCalculator(const float epsilon) : epsilon(epsilon) {
        if (this->epsilon < 0) {
            throw std::invalid_argument("Epsilon is less than 0.");
        }
    }

    std::unique_ptr<Subset> getApproximationSet(
        std::unique_ptr<MutableSubset> consumer, 
        RelevanceCalculator& calc,
        const BaseData &data, 
        size_t k
    ) {
        std::unordered_set<size_t> seen;

        std::unique_ptr<NaiveKernelMatrix> kernelMatrix(NaiveKernelMatrix::from(data, calc));
        spdlog::debug("created fast kernel matrix");
        
        std::vector<float> diagonals = kernelMatrix->getDiagonals(); 
        spdlog::debug("created fast kernel matrix");

        std::vector<std::vector<float>> c(data.totalRows(), std::vector<float>());

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
                
                float e = (kernelMatrix->get(j, i) - KernelMatrix::getDotProduct(c[j], c[i])) / std::sqrt(diagonals[j]);
                c[i].push_back(e);
                diagonals[i] -= std::pow(e, 2);
            }

            bestScore = getNextHighestScore(diagonals, seen);

            // To ensure "Numerical stability" ¯\_(ツ)_/¯
            if (bestScore.second <= this->epsilon) {
                spdlog::info("score of {0:f} was less than {1:f}", bestScore.second, this->epsilon);
                return MutableSubset::upcast(move(consumer));
            }

            j = bestScore.first;
            seen.insert(j);
            consumer->addRow(j, bestScore.second);
        }
    
        return MutableSubset::upcast(move(consumer));
    }
};