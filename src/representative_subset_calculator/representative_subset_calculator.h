#include <vector>

class SubsetCalculator {
    public:
    virtual std::unique_ptr<Subset> getApproximationSet(const Data &data, size_t k) = 0; 
};

class RowCalculator {

};