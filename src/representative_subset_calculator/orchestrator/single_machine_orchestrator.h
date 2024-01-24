#include <string>
#include <CLI/CLI.hpp>
#include "nlohmann/json.hpp"
#include "orchestrator.h"

struct appData {
    std::string inputFile;
    std::string outputFile;
    size_t outputSetSize;
    bool binaryInput = false;
    bool normalizeInput = false;
    double epsilon = -1;
    unsigned int algorithm;
} typedef AppData;

class SingleMachineOrchestrator : public Orchestrator{
    private:
    AppData appData;
    Timers timers;

    public:
    SingleMachineOrchestrator(AppData &appData) : appData(appData) {}

    static Orchestrator* build(int argc, char** argv, CLI::App &app) {
        AppData appData;
        addCmdOptions(app, appData);
        return (Orchestrator*)(new SingleMachineOrchestrator(appData));
    }

    void runJob() {
        std::ifstream inputFile;
        inputFile.open(this->appData.inputFile);
        DataLoader *dataLoader = buildDataLoader(this->appData, inputFile);
        Data data = DataBuilder::buildData(*dataLoader);
        inputFile.close();

        std::cout << "Finding a representative set for " << data.rows << " rows and " << data.columns << " columns" << std::endl;

        RepresentativeSubsetCalculator* calculator = getCalculator(this->appData, this->timers);
        RepresentativeSubset subset = calculator->getApproximationSet(data, appData.outputSetSize);

        nlohmann::json output = SingleMachineOrchestrator::buildOutput(appData, subset, timers);
        std::ofstream outputFile;
        outputFile.open(appData.outputFile);
        outputFile << output.dump(2);
        outputFile.close();

        delete calculator;
        delete dataLoader;
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

    static nlohmann::json buildOutput(
        const AppData &appData, 
        const RepresentativeSubset &subset,
        const Timers &timers) {
        nlohmann::json output {
            {"k", appData.outputSetSize}, 
            {"algorithm", algorithmToString(appData)},
            {"epsilon", appData.epsilon},
            {"RepresentativeRows", subset.representativeRows},
            {"Coverage", subset.coverage},
            {"timings", timers.outputToJson()}
        };

        return output;
    }

    static void addCmdOptions(CLI::App &app, AppData &appData) {
        app.add_option("-i,--input", appData.inputFile, "Path to input file. Should contain data in row vector format.")->required();
        app.add_option("-o,--output", appData.outputFile, "Path to output file.")->required();
        app.add_option("-k,--outputSetSize", appData.outputSetSize, "Sets the desired size of the representative set.")->required();
        app.add_option("-e,--epsilon", appData.epsilon, "Only used for the fast greedy variants. Determines the threshold for when seed selection is terminated.");
        app.add_option("-a,--algorithm", appData.algorithm, "Determines the seed selection algorithm. 0) naive, 1) lazy, 2) fast greedy, 3) lazy fast greedy");
        app.add_flag("--loadBinary", appData.binaryInput, "Use this flag if you want to load a binary input file.");
        app.add_flag("--normalizeInput", appData.normalizeInput, "Use this flag to normalize each input vector.");
    }

    static DataLoader* buildDataLoader(const AppData &appData, std::istream &data) {
        DataLoader *base = appData.binaryInput ? (DataLoader*)(new BinaryDataLoader(data)) : (DataLoader*)(new AsciiDataLoader(data));
        return appData.normalizeInput ? (DataLoader*)(new Normalizer(*base)) : base;
    }

    static RepresentativeSubsetCalculator* getCalculator(const AppData &appData, Timers &timers) {
        switch (appData.algorithm) {
            case 0:
                return (RepresentativeSubsetCalculator*)(new NaiveRepresentativeSubsetCalculator(timers));
            case 1:
                return (RepresentativeSubsetCalculator*)(new LazyRepresentativeSubsetCalculator(timers));
            case 2:
                return (RepresentativeSubsetCalculator*)(new FastRepresentativeSubsetCalculator(timers, appData.epsilon));
            default:
                throw new std::invalid_argument("Could not find algorithm");
        }
    }
};