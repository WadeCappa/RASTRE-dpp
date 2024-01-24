#include "nlohmann/json.hpp"
#include "orchestrator.h"

class SingleMachineOrchestrator : public Orchestrator{
    private:
    Timers timers;

    public:
    RepresentativeSubset getApproximationSet(const Data &data, const AppData &appData) {
        RepresentativeSubsetCalculator* calculator = getCalculator(appData, this->timers);
        RepresentativeSubset subset = calculator->getApproximationSet(data, appData.outputSetSize);
        delete calculator;
        return subset;
    }

    nlohmann::json buildOutput(const RepresentativeSubset &subset, const AppData &appData) {
        nlohmann::json output {
            {"k", appData.outputSetSize}, 
            {"algorithm", algorithmToString(appData)},
            {"epsilon", appData.epsilon},
            {"RepresentativeRows", subset.representativeRows},
            {"Coverage", subset.coverage},
            {"timings", this->timers.outputToJson()}
        };

        return output;
    }

    private:
    static std::string algorithmToString(const AppData &appData) {
        switch (appData.algorithm) {
            case 0:
                return "naive greedy";
            case 1:
                return "lazy greedy";
            case 2:
                return "fast greedy";
            case 3:
                return "lazy fast greedy";
            default:
                throw new std::invalid_argument("Could not find algorithm");
        }
    }
};