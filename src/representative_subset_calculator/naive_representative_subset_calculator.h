#include <vector>
#include <stdexcept>
#include <set>
#include <limits>

#include "timers/timers.h"
#include "similarity_matrix/similarity_matrix.h"

#include "representative_subset_calculator.h"

class NaiveRepresentativeSubsetCalculator : public RepresentativeSubsetCalculator {
    private: 
    Timers &timers;

    RepresentativeSubset buildResult(const std::vector<size_t> &rows, double coverage) {
        RepresentativeSubset subset;
        subset.representativeRows = rows;
        subset.coverage = coverage;
        return subset;
    }

    public:
    NaiveRepresentativeSubsetCalculator(Timers &timers) : timers(timers) {}

    RepresentativeSubset getApproximationSet(const Data &data, size_t k) {
        timers.totalCalculationTime.startTimer();

        std::vector<size_t> res;
        SimilarityMatrix matrix; 
        std::set<size_t> seen;

        double currentScore = 0;

        for (size_t seed = 0; seed < k; seed++) {
            std::vector<double> marginals(data.rows);

            #pragma omp parallel for 
            for (size_t index = 0; index < data.rows; index++) {
                const auto & row = data.data[index];
                SimilarityMatrix tempMatrix(matrix);
                tempMatrix.addRow(row);
                marginals[index] = tempMatrix.getCoverage();
            }

            double highestMarginal = -std::numeric_limits<double>::max();
            double changeInMarginal = 0;
            size_t bestRow = -1;
            for (size_t index = 0; index < data.rows; index++) {
                if (seen.find(index) != seen.end()) {
                    continue;
                }

                const auto &tempMarginal = marginals[index];
                if (tempMarginal > highestMarginal && tempMarginal - currentScore > 0) {
                    bestRow = index;
                    highestMarginal = tempMarginal;
                }
            }

            if (bestRow == -1) {
                std::cout << "FAILED to add element to matrix that increased marginal" << std::endl;
                timers.totalCalculationTime.stopTimer();
                return buildResult(res, matrix.getCoverage());
            }

            std::cout << "naive found " << bestRow << " which increased marginal score by " << highestMarginal - currentScore << std::endl;
            res.push_back(bestRow);
            matrix.addRow(data.data[bestRow]);
            seen.insert(bestRow);
            currentScore = highestMarginal;
        }

        timers.totalCalculationTime.stopTimer();
        return buildResult(res, matrix.getCoverage());
    }
};