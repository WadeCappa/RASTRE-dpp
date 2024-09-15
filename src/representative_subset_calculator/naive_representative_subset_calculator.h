#include <vector>
#include <stdexcept>
#include <set>
#include <limits>

#include "similarity_matrix/similarity_matrix.h"
#include "representative_subset_calculator.h"

class NaiveSubsetCalculator : public SubsetCalculator {
    private: 

    public:
    NaiveSubsetCalculator() {}

    std::unique_ptr<Subset> getApproximationSet(
        std::unique_ptr<MutableSubset> consumer, 
        const BaseData &data, 
        size_t k
    ) {
        MutableSimilarityMatrix matrix; 
        std::set<size_t> seen;

        float currentScore = 0;

        for (size_t seed = 0; seed < k; seed++) {
            std::vector<float> marginals(data.totalRows());

            #pragma omp parallel for 
            for (size_t index = 0; index < data.totalRows(); index++) {
                // TODO: Use the kernel matrix here, keep diagonals as state
                MutableSimilarityMatrix tempMatrix(matrix);
                tempMatrix.addRow(data.getRow(index));
                marginals[index] = tempMatrix.getCoverage();
            }

            float highestMarginal = -std::numeric_limits<float>::max();
            float changeInMarginal = 0;
            size_t bestRow = -1;
            for (size_t index = 0; index < data.totalRows(); index++) {
                if (seen.find(index) != seen.end()) {
                    continue;
                }

                const auto &tempMarginal = marginals[index];
                if (tempMarginal > highestMarginal) {
                    bestRow = index;
                    highestMarginal = tempMarginal;
                }
            }

            if (bestRow == -1) {
                spdlog::error("FAILED to add element to matrix that increased marginal");
                return MutableSubset::upcast(move(consumer));
            }

            float marginalGain = highestMarginal - currentScore;
            consumer->addRow(bestRow, marginalGain);
            matrix.addRow(data.getRow(bestRow));
            seen.insert(bestRow);
            currentScore = highestMarginal;
        }

        return MutableSubset::upcast(move(consumer));
    }
};