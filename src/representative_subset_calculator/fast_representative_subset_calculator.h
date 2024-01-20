#include "representative_subset_calculator"

class FastRepresentativeSubsetCalculator : public RepresentativeSubsetCalculator {
    private:
    Timers &timers;

    public:
    FastRepresentativeSubsetCalculator(Timers &timers) : timers(timers) {}

    RepresentativeSubset getApproximationSet(const Data &data, size_t k) {
    

    
        RepresentativeSubset subset;
        subset.representativeRows = subsetRows;
        subset.coverage = matrix.getCoverage();
        timers.totalCalculationTime.stopTimer();
        return subset;
    }
};