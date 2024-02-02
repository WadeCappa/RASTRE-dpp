#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <limits.h>

#include<cstdlib>

#include <random>

struct appData{
    std::string inputFile;
    std::string outputFile;
    size_t outputSetSize;
    bool binaryInput = false;
    bool normalizeInput = false;
    double epsilon = -1;
    unsigned int algorithm;

    int worldSize = 1;
    int worldRank = 0;
    size_t numberOfDataRows;
} typedef AppData;

class Orchestrator {
    public:
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

    static double getTotalCoverage(const std::vector<std::pair<size_t, double>> &solution) {
        double totalCoverage = 0;
        for (const auto & s : solution) {
            totalCoverage += s.second;
        }

        return totalCoverage;
    }

    static nlohmann::json solutionToJson(
        const std::vector<std::pair<size_t, double>> &solution
    ) {
        std::vector<size_t> rows;
        std::vector<double> marginals;

        for (const auto & s : solution) {
            rows.push_back(s.first);
            marginals.push_back(s.second);
        }

        nlohmann::json output {
            {"rows", rows}, 
            {"marginalGains", marginals}, 
            {"totalCoverage", getTotalCoverage(solution)}
        };

        return output;
    }

    static nlohmann::json buildDatasetJson(const Data &data, const AppData &appData) {
        nlohmann::json output {
            {"rows", data.totalRows()},
            {"columns", data.totalColumns()},
            {"inputFile", appData.inputFile}
        };

        return output;
    }

    static nlohmann::json buildOutput(
        const AppData &appData, 
        const std::vector<std::pair<size_t, double>> &solution,
        const Data &data,
        const Timers &timers
    ) {
        nlohmann::json output {
            {"k", appData.outputSetSize}, 
            {"algorithm", algorithmToString(appData)},
            {"epsilon", appData.epsilon},
            {"RepresentativeRows", solutionToJson(solution)},
            {"timings", timers.outputToJson()},
            {"dataset", buildDatasetJson(data, appData)},
            {"worldSize", appData.worldSize}
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
        DataLoader *dataLoader = appData.binaryInput ? dynamic_cast<DataLoader*>(new BinaryDataLoader(data)) : dynamic_cast<DataLoader*>(new AsciiDataLoader(data));

        if (appData.normalizeInput) {
            dataLoader = dynamic_cast<DataLoader*>(new Normalizer(*dataLoader));
        }

        return dataLoader;
    }
            
    static DataLoader* buildMpiDataLoader(
        const AppData &appData, 
        std::istream &data, 
        const std::vector<unsigned int> &rankMapping
    ) {
        DataLoader *dataLoader = Orchestrator::buildDataLoader(appData, data);
        return dynamic_cast<DataLoader*>(new BlockedDataLoader(*dataLoader, rankMapping, appData.worldRank));
    }


    static RepresentativeSubsetCalculator* getCalculator(const AppData &appData, Timers &timers) {
        switch (appData.algorithm) {
            case 0:
                return dynamic_cast<RepresentativeSubsetCalculator*>(new NaiveRepresentativeSubsetCalculator(timers));
            case 1:
                return dynamic_cast<RepresentativeSubsetCalculator*>(new LazyRepresentativeSubsetCalculator(timers));
            case 2:
                return dynamic_cast<RepresentativeSubsetCalculator*>(new FastRepresentativeSubsetCalculator(timers, appData.epsilon));
            case 3: 
                return dynamic_cast<RepresentativeSubsetCalculator*>(new LazyFastRepresentativeSubsetCalculator(timers, appData.epsilon));
            default:
                throw new std::invalid_argument("Could not find algorithm");
        }
    }

    // TODO: This can be improved. Instead of returning a vector that is the same size of all rows that includes all rank data, return only
    //  the rows that this given rank cares about. This will improve performance while loading the dataset.
    static std::vector<unsigned int> getRowToRank(const AppData &appData, const int seed) {
        std::vector<unsigned int> rowToRank(appData.numberOfDataRows, -1);
        std::uniform_int_distribution<int> uniform_distribution(0, appData.worldSize - 1);
        std::default_random_engine number_selecter(seed);

        for (int i = 0; i < appData.numberOfDataRows; i++) {
            rowToRank[i] = uniform_distribution(number_selecter);
        }

        return rowToRank;
    }
};