#include <vector>
#include <math.h>
#include <unordered_set>

#include "../data_tools/base_data.h"
#include "representative_subset_calculator.h"
#include "kernel_matrix/relevance_calculator.h"
#include "kernel_matrix/kernel_matrix.h"
#include "representative_subset.h"

#ifndef FAST_REPRESENTATIVE_SUBSET_CALCULATOR_H
#define FAST_REPRESENTATIVE_SUBSET_CALCULATOR_H

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
            const float score = diagonals[i];
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
        std::vector<std::vector<float>> c(data.totalRows(), std::vector<float>());

        auto bestScore = getNextHighestScore(diagonals, seen);
        SPDLOG_TRACE("first seed is {0:d} of score {1:f}", bestScore.first, bestScore.second);
        size_t j = bestScore.first;
        seen.insert(j);
        consumer->addRow(j, bestScore.second);

        while (consumer->size() < k) {
            #pragma omp parallel for 
            for (size_t i = 0; i < data.totalRows(); i++) {
                if (seen.find(i) != seen.end()) {
                    continue;
                }
                
                const float dot_product = KernelMatrix::getDotProduct(c[j], c[i]);
                const float e = (kernelMatrix->get(j, i) - dot_product) / std::sqrt(diagonals[j]);
                c[i].push_back(e);
                diagonals[i] -= std::pow(e, 2);
            }

            bestScore = getNextHighestScore(diagonals, seen);
            SPDLOG_TRACE("next best score of {0:f} with seed {1:d}", bestScore.second, bestScore.first);

            if (bestScore.second <= this->epsilon) {
                spdlog::warn("score of {0:f} was less than {1:f}", bestScore.second, this->epsilon);
                return MutableSubset::upcast(std::move(consumer));
            }

            j = bestScore.first;
            seen.insert(j);
            consumer->addRow(j, bestScore.second);
        }
    
        return MutableSubset::upcast(std::move(consumer));
    }
};

#endif