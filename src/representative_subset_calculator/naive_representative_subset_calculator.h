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

        for (size_t seed = 0; seed < k; seed++) {
            double highestScore = -std::numeric_limits<double>::max();
            size_t bestRow = -1;
            // TODO: do in parallel
            for (size_t index = 0; index < data.rows; index++) {
                if (seen.find(index) != seen.end()) {
                    continue;
                }

                const auto & row = data.data[index];
                SimilarityMatrix tempMatrix(matrix);
                tempMatrix.addRow(row);
                double tempScore = tempMatrix.getCoverage();

                if (tempScore > highestScore) {
                    bestRow = index;
                    highestScore = tempScore;
                }
            }

            if (bestRow == -1) {
                timers.totalCalculationTime.stopTimer();
                return buildResult(res, matrix.getCoverage());
            }

            std::cout << "found " << bestRow << " with score of " << highestScore << std::endl;
            res.push_back(bestRow);
            matrix.addRow(data.data[bestRow]);
            seen.insert(bestRow);
        }

        timers.totalCalculationTime.stopTimer();
        return buildResult(res, matrix.getCoverage());
    }
};