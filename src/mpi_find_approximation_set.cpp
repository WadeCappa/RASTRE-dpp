#include "data_tools/normalizer.h"
#include "data_tools/matrix_builder.h"
#include "representative_subset_calculator/naive_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_representative_subset_calculator.h"
#include "representative_subset_calculator/fast_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_fast_representative_subset_calculator.h"
#include "representative_subset_calculator/orchestrator/mpi_orchestrator.h"

#include <CLI/CLI.hpp>
#include "nlohmann/json.hpp"

#include <mpi.h>


int main(int argc, char** argv) {
    CLI::App app{"Approximates the best possible approximation set for the input dataset using MPI."};
    AppData appData;
    MpiOrchestrator::addMpiCmdOptions(app, appData);
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
    std::vector<std::pair<size_t, double>> localSolution = calculator->getApproximationSet(data, appData.outputSetSize);

    // TODO: batch this into blocks using a custom MPI type to send higher volumes of data.
    unsigned int sendDataSize = MpiOrchestrator::getTotalSendData(data, localSolution);
    std::vector<int> receivingDataSizesBuffer(appData.worldSize, 0);
    MPI_Gather(&sendDataSize, 1, MPI_INT, receivingDataSizesBuffer.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    std::vector<double> sendBuffer;
    MpiOrchestrator::buildSendBuffer(data, localSolution, sendBuffer, sendDataSize);

    std::vector<double> receiveBuffer;
    std::vector<int> displacements;
    if (appData.worldRank == 0) {
        MpiOrchestrator::buildReceiveBuffer(receivingDataSizesBuffer, receiveBuffer);
        MpiOrchestrator::buildDisplacementBuffer(receivingDataSizesBuffer, displacements);
    }

    MPI_Gatherv(
        sendBuffer.data(), 
        sendBuffer.size(), 
        MPI_DOUBLE, 
        receiveBuffer.data(), 
        receivingDataSizesBuffer.data(), 
        displacements.data(),
        MPI_DOUBLE, 
        0, 
        MPI_COMM_WORLD
    );

    if (appData.worldRank == 0) {
        std::vector<std::pair<size_t, std::vector<double>>> newData;
        MpiOrchestrator::rebuildData(receiveBuffer, data.totalColumns(), newData);
        SelectiveData bestRows(newData);

        std::vector<std::pair<size_t, double>> globalSolutionWithLocalIndicies = calculator->getApproximationSet(bestRows, appData.outputSetSize);
        std::vector<std::pair<size_t, double>> solution = bestRows.translateSolution(globalSolutionWithLocalIndicies);

        nlohmann::json result = MpiOrchestrator::buildOutput(appData, solution, data, timers);

        std::ofstream outputFile;
        outputFile.open(appData.outputFile);
        outputFile << result.dump(2);
        outputFile.close();
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}