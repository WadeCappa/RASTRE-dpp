#include "representative_subset_calculator.h"

class LazyRepresentativeSubsetCalculator : public RepresentativeSubsetCalculator {
    private:
    Timers &timers;

    public:
    LazyRepresentativeSubsetCalculator(Timers &timers) : timers(timers) {}

    RepresentativeSubset getApproximationSet(const Data &data, size_t k) {

    }
};