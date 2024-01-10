#include <vector>

#include "similarity_matrix/similarity_matrix.h"

class RepresentativeSubsetCalculator {
    public:
    virtual std::vector<size_t> getApproximationSet(const Data &data, size_t k) = 0; 
};

class NaiveRepresentativeSubsetCalculator : public RepresentativeSubsetCalculator {
    public:
    std::vector<size_t> getApproximationSet(const Data &data, size_t k) {
        std::vector<size_t> res(k);
        SimilarityMatrix representativeMatrix; 
        for (size_t seed = 0; seed < k; seed++) {
            double highestScore = 0;
            size_t bestRow = 0;
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

        return res;
    }
};