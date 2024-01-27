#include "data_tools/normalizer.h"
#include "data_tools/matrix_builder.h"
#include "representative_subset_calculator/naive_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_representative_subset_calculator.h"
#include "representative_subset_calculator/fast_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_fast_representative_subset_calculator.h"

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

struct appData {
    std::string inputFile;
    std::string outputFile;
    size_t outputSetSize;
    bool binaryInput = false;
    bool normalizeInput = false;
    double epsilon = -1;
    unsigned int algorithm;
} typedef AppData;

std::string algorithmToString(const AppData &appData) {
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

nlohmann::json buildRepresentativeSubsetOutput(
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
    };

    return output;

}

nlohmann::json buildOutput(
    const AppData &appData, 
    const std::vector<std::pair<size_t, double>> &solution,
    const Timers &timers
) {
    nlohmann::json output {
        {"k", appData.outputSetSize}, 
        {"algorithm", algorithmToString(appData)},
        {"epsilon", appData.epsilon},
        {"RepresentativeRows", buildRepresentativeSubsetOutput(solution)},
        {"timings", timers.outputToJson()}
    };

    return output;
}

void addCmdOptions(CLI::App &app, AppData &appData) {
    app.add_option("-i,--input", appData.inputFile, "Path to input file. Should contain data in row vector format.")->required();
    app.add_option("-o,--output", appData.outputFile, "Path to output file.")->required();
    app.add_option("-k,--outputSetSize", appData.outputSetSize, "Sets the desired size of the representative set.")->required();
    app.add_option("-e,--epsilon", appData.epsilon, "Only used for the fast greedy variants. Determines the threshold for when seed selection is terminated.");
    app.add_option("-a,--algorithm", appData.algorithm, "Determines the seed selection algorithm. 0) naive, 1) lazy, 2) fast greedy, 3) lazy fast greedy");
    app.add_flag("--loadBinary", appData.binaryInput, "Use this flag if you want to load a binary input file.");
    app.add_flag("--normalizeInput", appData.normalizeInput, "Use this flag to normalize each input vector.");
}

DataLoader* buildDataLoader(const AppData &appData, std::istream &data) {
    DataLoader *base = appData.binaryInput ? (DataLoader*)(new BinaryDataLoader(data)) : (DataLoader*)(new AsciiDataLoader(data));
    return appData.normalizeInput ? (DataLoader*)(new Normalizer(*base)) : base;
}

RepresentativeSubsetCalculator* getCalculator(const AppData &appData, Timers &timers) {
    switch (appData.algorithm) {
        case 0:
            return (RepresentativeSubsetCalculator*)(new NaiveRepresentativeSubsetCalculator(timers));
        case 1:
            return (RepresentativeSubsetCalculator*)(new LazyRepresentativeSubsetCalculator(timers));
        case 2:
            return (RepresentativeSubsetCalculator*)(new FastRepresentativeSubsetCalculator(timers, appData.epsilon));
        case 3: 
            return (RepresentativeSubsetCalculator*)(new LazyFastRepresentativeSubsetCalculator(timers, appData.epsilon));
        default:
            throw new std::invalid_argument("Could not find algorithm");
    }
}

int main(int argc, char** argv) {
    CLI::App app{"Approximates the best possible approximation set for the input dataset."};
    AppData appData;
    addCmdOptions(app, appData);
    CLI11_PARSE(app, argc, argv);

    std::ifstream inputFile;
    inputFile.open(appData.inputFile);
    DataLoader *dataLoader = buildDataLoader(appData, inputFile);
    Data data(*dataLoader);
    inputFile.close();

    std::cout << "Finding a representative set for " << data.rows << " rows and " << data.columns << " columns" << std::endl;

    Timers timers;
    RepresentativeSubsetCalculator *calculator = getCalculator(appData, timers);
    auto solution = calculator->getApproximationSet(data, appData.outputSetSize);

    nlohmann::json output = buildOutput(appData, solution, timers);
    std::ofstream outputFile;
    outputFile.open(appData.outputFile);
    outputFile << output.dump(2);
    outputFile.close();

    return 0;
}