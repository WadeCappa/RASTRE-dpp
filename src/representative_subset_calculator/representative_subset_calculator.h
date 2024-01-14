#include <vector>

typedef struct representativeSubset {
    std::vector<size_t> representativeRows;
    double coverage;
} RepresentativeSubset;

class RepresentativeSubsetCalculator {
    public:
    virtual RepresentativeSubset getApproximationSet(const Data &data, size_t k) = 0; 
};
