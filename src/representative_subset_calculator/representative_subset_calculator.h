#include <vector>

class RepresentativeSubsetCalculator {
    public:
    virtual std::unique_ptr<RepresentativeSubset> getApproximationSet(const Data &data, size_t k) = 0; 
};