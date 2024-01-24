#include "orchestrator.h"

class MpiOrchestrator : public Orchestrator {
    private:
    Orchestrator &base;

    public:
    MpiOrchestrator(Orchestrator &base) : base(base) {}

    RepresentativeSubset getApproximationSet(const Data &data, const AppData &appData) {
        return base.getApproximationSet(data, appData);
    }

    nlohmann::json buildOutput(const RepresentativeSubset &subset, const AppData &appData) {
        return base.buildOutput(subSet, appData);
    }
}