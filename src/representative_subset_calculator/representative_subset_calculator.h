#include <vector>
#include <stdexcept>
#include <set>
#include <limits>

#include "timers/timers.h"
#include "similarity_matrix/similarity_matrix.h"

typedef struct representativeSubset {
    std::vector<size_t> representativeRows;
    double coverage;
} RepresentativeSubset;

class RepresentativeSubsetCalculator {
    public:
    virtual RepresentativeSubset getApproximationSet(const Data &data, size_t k) = 0; 
};
