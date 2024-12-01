#include <vector>

class SubsetCalculator {
    public:
    virtual ~SubsetCalculator() {}
    virtual std::unique_ptr<Subset> getApproximationSet(std::unique_ptr<MutableSubset> consumer, const BaseData &data, size_t k) = 0; 

    std::unique_ptr<Subset> getApproximationSet(const BaseData &data, size_t k) {
        return this->getApproximationSet(move(NaiveMutableSubset::makeNew()), data, k);
    }
};

/**
 * This is the null calculator; all seeds are added into the subset regardless of k
 */
class AddAllToSubsetCalculator : public SubsetCalculator {
    public:
    std::unique_ptr<Subset> getApproximationSet(std::unique_ptr<MutableSubset> consumer, const BaseData &data, size_t _k) {
        size_t rows = data.totalRows();
        for (size_t i = 0; i < rows; i++) {
            // Marginal gain shouldn't matter here
            consumer->addRow(i, 0);
        }
    
        return MutableSubset::upcast(move(consumer));
    }
};