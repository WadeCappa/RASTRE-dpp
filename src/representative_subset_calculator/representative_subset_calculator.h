#include <vector>

class RepresentativeSubsetCalculator {
    public:
    virtual std::vector<std::pair<size_t, double>> getApproximationSet(const Data &data, size_t k) = 0; 
};
