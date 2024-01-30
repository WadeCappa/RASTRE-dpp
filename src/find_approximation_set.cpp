#include "data_tools/normalizer.h"
#include "data_tools/matrix_builder.h"
#include "representative_subset_calculator/naive_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_representative_subset_calculator.h"
#include "representative_subset_calculator/fast_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_fast_representative_subset_calculator.h"
#include "representative_subset_calculator/orchestrator/orchestrator.h"

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

int main(int argc, char** argv) {
    CLI::App app{"Approximates the best possible approximation set for the input dataset."};
    AppData appData;
    Orchestrator::addCmdOptions(app, appData);
    CLI11_PARSE(app, argc, argv);

    std::ifstream inputFile;
    inputFile.open(appData.inputFile);
    DataLoader *dataLoader = Orchestrator::buildDataLoader(appData, inputFile);
    NaiveData data(*dataLoader);
    inputFile.close();

    delete dataLoader;

    Timers timers;
    RepresentativeSubsetCalculator *calculator = Orchestrator::getCalculator(appData, timers);
    std::vector<std::pair<size_t, double>> solution = calculator->getApproximationSet(data, appData.outputSetSize);
    nlohmann::json result = Orchestrator::buildOutput(appData, solution, data, timers);

    std::ofstream outputFile;
    outputFile.open(appData.outputFile);
    outputFile << result.dump(2);
    outputFile.close();

    return 0;
}