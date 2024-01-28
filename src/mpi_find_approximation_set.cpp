#include "data_tools/normalizer.h"
#include "data_tools/matrix_builder.h"
#include "representative_subset_calculator/naive_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_representative_subset_calculator.h"
#include "representative_subset_calculator/fast_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_fast_representative_subset_calculator.h"
#include "representative_subset_calculator/orchestrator/orchestrator.h"

#include <CLI/CLI.hpp>
#include "nlohmann/json.hpp"

#include <mpi.h>


static void addMpiCmdOptions(CLI::App &app, AppData &appData) {
    app.add_option("-n,--numberOfRows", appData.numberOfDataRows, "The number of total rows of data in your input file.")->required();
}

int main(int argc, char** argv) {
    CLI::App app{"Approximates the best possible approximation set for the input dataset using MPI."};
    AppData appData;
    Orchestrator::addCmdOptions(app, appData);
    CLI11_PARSE(app, argc, argv);

    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &appData.worldRank);
    MPI_Comm_size(MPI_COMM_WORLD, &appData.worldSize);

    unsigned int seed = (unsigned int)time(0);
    MPI_Bcast(&seed, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);

    std::vector<unsigned int> rowToRank = Orchestrator::getRowToRank(appData, seed);

    std::ifstream inputFile;
    inputFile.open(appData.inputFile);
    DataLoader *dataLoader = Orchestrator::buildMpiDataLoader(appData, inputFile, rowToRank);
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