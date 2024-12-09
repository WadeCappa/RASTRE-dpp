#include <vector>

class SubsetCalculator {
    public:
    virtual ~SubsetCalculator() {}
    virtual std::unique_ptr<Subset> getApproximationSet(
        std::unique_ptr<MutableSubset> consumer, 
        std::unique_ptr<RelevanceCalculator> calc,
        const BaseData &data, 
        size_t k
    ) = 0; 

    std::unique_ptr<Subset> getApproximationSet(const BaseData &data, size_t k) {
        return SubsetCalculator::getApproximationSet(NaiveRelevanceCalculator::from(data), data, k);
    }

    std::unique_ptr<Subset> getApproximationSet(
        std::unique_ptr<RelevanceCalculator> calc, 
        const BaseData &data, 
        size_t k) {
        return this->getApproximationSet(
            NaiveMutableSubset::makeNew(), 
            move(calc),
            data, 
            k
        );
    }
};

/**
 * This is the null calc; all seeds are added into the subset regardless of k. 
 * 
 * N.B, this will not calculate the score of each seed since the score only makes since 
 * given an incremental workload of adding each seed depending on the previous score. 
 * This means the calc param is unused here, you can pass in a nullptr
 */
class AddAllToSubsetCalculator : public SubsetCalculator {
    public:
    std::unique_ptr<Subset> getApproximationSet(
        std::unique_ptr<MutableSubset> consumer, 
        std::unique_ptr<RelevanceCalculator> _calculator,
        const BaseData &data, 
        size_t _k
    ) {
        size_t rows = data.totalRows();
        for (size_t i = 0; i < rows; i++) {
            // Marginal gain shouldn't matter here
            consumer->addRow(i, 0);
        }
    
        return MutableSubset::upcast(move(consumer));
    }
};