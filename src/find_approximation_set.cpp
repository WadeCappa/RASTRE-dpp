#include "data_tools/normalizer.h"
#include "data_tools/matrix_builder.h"
#include "representative_subset_calculator/naive_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_representative_subset_calculator.h"

#include <CLI/CLI.hpp>
#include "nlohmann/json.hpp"

struct appData {
    std::string inputFile;
    std::string outputFile;
    size_t outputSetSize;
    bool binaryInput = false;
    bool normalizeInput = false;
} typedef AppData;

nlohmann::json buildOutput(
    const AppData &appData, 
    const RepresentativeSubset &subset,
    const Timers &timers) {
    nlohmann::json output {
        {"k", appData.outputSetSize}, 
        {"RepresentativeRows", subset.representativeRows},
        {"Coverage", subset.coverage},
        {"timings", timers.outputToJson()}
    };

    return output;
}

void addCmdOptions(CLI::App &app, AppData &appData) {
    app.add_option("-i,--input", appData.inputFile, "Path to input file. Should contain data in row vector format.")->required();
    app.add_option("-o,--output", appData.outputFile, "Path to output file.")->required();
    app.add_option("-k,--outputSetSize", appData.outputSetSize, "Sets the desired size of the representative set.")->required();
    app.add_flag("--loadBinary", appData.binaryInput, "Use this flag if you want to load a binary input file.");
    app.add_flag("--normalizeInput", appData.normalizeInput, "Use this flag to normalize each input vector.");
}

DataLoader* buildDataLoader(const AppData &appData, std::istream &data) {
    DataLoader *base = appData.binaryInput ? (DataLoader*)(new BinaryDataLoader(data)) : (DataLoader*)(new AsciiDataLoader(data));
    return appData.normalizeInput ? (DataLoader*)(new Normalizer(*base)) : base;
}

RepresentativeSubsetCalculator* getCalculator(Timers &timers) {
    return true ? (RepresentativeSubsetCalculator*)(new LazyRepresentativeSubsetCalculator(timers)) : (RepresentativeSubsetCalculator*)(new NaiveRepresentativeSubsetCalculator (timers));
}

int main(int argc, char** argv) {
    CLI::App app{"Approximates the best possible approximation set for the input dataset."};
    AppData appData;
    addCmdOptions(app, appData);
    CLI11_PARSE(app, argc, argv);

    std::ifstream inputFile;
    inputFile.open(appData.inputFile);
    DataLoader *dataLoader = buildDataLoader(appData, inputFile);
    Data data = DataBuilder::buildData(*dataLoader);
    inputFile.close();

    std::cout << "Finding a representative set for " << data.rows << " rows and " << data.columns << " columns" << std::endl;

    Timers timers;
    RepresentativeSubsetCalculator *calculator = getCalculator(timers);
    RepresentativeSubset subset = calculator->getApproximationSet(data, appData.outputSetSize);

    nlohmann::json output = buildOutput(appData, subset, timers);
    std::ofstream outputFile;
    outputFile.open(appData.outputFile);
    outputFile << output.dump(2);
    outputFile.close();

    return 0;
}