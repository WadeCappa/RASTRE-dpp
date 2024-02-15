
#include <vector>

#include "similarity_matrix/similarity_matrix.h"
#include "representative_subset_calculator.h"

typedef struct origin {
    int source;
    int seed;
} Origin;


class ThresholdBucket 
{
    private:
    double utility;
    std::vector<std::pair<size_t, double>> solution; 
    double marginalGainThreshold;
    int k;
    SimilarityMatrix matrix;

    public:
    ThresholdBucket(size_t deltaZero, int k, double epsilon, size_t numBucket) 
    {
        this->marginalGainThreshold = ( (double)deltaZero / (double)( 2 * k )) * (double)std::pow(1 + epsilon, numBucket);
        this->k = k;
        this->utility = 0;
    }

    size_t getUtility() 
    {
        return this->utility;
    }

    std::vector<std::pair<unsigned int, size_t>> getSolution()
    {
        return this->solution;
    }

    bool attemptInsert(std::pair<size_t, std::vector<double>> element) 
    {
        if (this->solution.size() >= this->k)
        {
            return false;
        }
       
        const auto & row = element.second;
        SimilarityMatrix tempMatrix(matrix);
        tempMatrix.addRow(row);

        auto marginal = tempMatrix.getCoverage();

        if (marginal >= this-> marginalGainThreshold) {
            this->solution.push_back(std::make_pair(element.first, marginal));
            this->utility += marginal;
            this->matrix.addRow(row);

            return true;
        }

        return false;
    }
};
