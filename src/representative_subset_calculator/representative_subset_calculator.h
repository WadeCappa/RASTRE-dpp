#include <vector>

#include "similarity_matrix/similarity_matrix.h"

typedef struct representativeSubset {
    std::vector<size_t> representativeRows;
    double coverage;
} RepresentativeSubset;

class RepresentativeSubsetCalculator {
    public:
    virtual RepresentativeSubset getApproximationSet(const Data &data, size_t k) = 0; 
};

class NaiveRepresentativeSubsetCalculator : public RepresentativeSubsetCalculator {
    public:
    RepresentativeSubset getApproximationSet(const Data &data, size_t k) {
        std::vector<size_t> res(k);
        SimilarityMatrix representativeMatrix; 
        for (size_t seed = 0; seed < k; seed++) {
            double highestScore = 0;
            size_t bestRow = 0;
            // TODO: do in parallel
            for (size_t index = 0; index < data.rows; index++) {
                const auto & row = data.data[index];
                SimilarityMatrix tempMatrix(representativeMatrix);
                tempMatrix.addRow(row);
                double tempScore = tempMatrix.getCoverage();
                if (tempScore > highestScore) {
                    bestRow = index;
                    highestScore = tempScore;
                }
            }

            res[seed] = bestRow;
            representativeMatrix.addRow(data.data[bestRow]);
        }

        RepresentativeSubset subset;
        subset.representativeRows = res;
        subset.coverage = representativeMatrix.getCoverage();
        return subset;
    }
};