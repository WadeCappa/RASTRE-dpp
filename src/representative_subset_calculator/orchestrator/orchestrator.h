#include <string>
#include "nlohmann/json.hpp"
#include <CLI/CLI.hpp>

struct appData {
    std::string inputFile;
    std::string outputFile;
    size_t outputSetSize;
    bool binaryInput = false;
    bool normalizeInput = false;
    double epsilon = -1;
    unsigned int algorithm;

    unsigned int worldSize = 1;
    unsigned int worldRank = 0;
} typedef AppData;

class Orchestrator {
    public:
    virtual RepresentativeSubset getApproximationSet(const Data &data, const AppData &appData) = 0;
    virtual nlohmann::json buildOutput(const RepresentativeSubset &subset, const AppData &appData) = 0;

    static void runJob(Orchestrator &job, const AppData &appData) {
        std::ifstream inputFile;
        inputFile.open(appData.inputFile);
        DataLoader *dataLoader = Orchestrator::buildDataLoader(appData, inputFile);
        Data data = DataBuilder::buildData(*dataLoader);
        inputFile.close();

        delete dataLoader;

        std::cout << "Finding a representative set for " << data.rows << " rows and " << data.columns << " columns" << std::endl;

        RepresentativeSubset solution = job.getApproximationSet(data, appData);
        nlohmann::json result = job.buildOutput(solution, appData);

        std::ofstream outputFile;
        outputFile.open(appData.outputFile);
        outputFile << result.dump(2);
        outputFile.close();
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