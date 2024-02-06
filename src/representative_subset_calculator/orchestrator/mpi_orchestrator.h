#include "orchestrator.h"


class MpiOrchestrator {
    public:
    static nlohmann::json buildMpiOutput(
        const AppData &appData, 
        const RepresentativeSubset &solution,
        const Data &data,
        const Timers &timers
    ) {
        nlohmann::json output = Orchestrator::buildOutputBase(appData, solution, data, timers);
        return output;
    }
};