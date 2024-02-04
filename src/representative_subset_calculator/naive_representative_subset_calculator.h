#include <vector>
#include <stdexcept>
#include <set>
#include <limits>

#include "similarity_matrix/similarity_matrix.h"
#include "representative_subset_calculator.h"

class NaiveRepresentativeSubsetCalculator : public RepresentativeSubsetCalculator {
    private: 

    public:
    NaiveRepresentativeSubsetCalculator() {}

    std::vector<std::pair<size_t, double>> getApproximationSet(const Data &data, size_t k) {
        std::vector<std::pair<size_t, double>> res;
        SimilarityMatrix matrix; 
        std::set<size_t> seen;

        double currentScore = 0;

        for (size_t seed = 0; seed < k; seed++) {
            std::vector<double> marginals(data.totalRows());

            #pragma omp parallel for 
            for (size_t index = 0; index < data.totalRows(); index++) {
                const auto & row = data.getRow(index);
                SimilarityMatrix tempMatrix(matrix);
                tempMatrix.addRow(row);
                marginals[index] = tempMatrix.getCoverage();
            }

            double highestMarginal = -std::numeric_limits<double>::max();
            double changeInMarginal = 0;
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
                std::cout << "FAILED to add element to matrix that increased marginal" << std::endl;
                return res;
            }

            double marginalGain = highestMarginal - currentScore;
            res.push_back(std::make_pair(bestRow, marginalGain));
            matrix.addRow(data.getRow(bestRow));
            seen.insert(bestRow);
            currentScore = highestMarginal;
        }

        return res;
    }
};