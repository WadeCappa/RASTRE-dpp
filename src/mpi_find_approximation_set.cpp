#include "data_tools/normalizer.h"
#include "data_tools/matrix_builder.h"
#include "representative_subset_calculator/timers/timers.h"
#include "representative_subset_calculator/naive_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_representative_subset_calculator.h"
#include "representative_subset_calculator/fast_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_fast_representative_subset_calculator.h"
#include "representative_subset_calculator/representative_subset.h"
#include "representative_subset_calculator/orchestrator/orchestrator.h"

#include "representative_subset_calculator/buffers/bufferBuilder.h"

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>


#include <mpi.h>

int main(int argc, char** argv) {
    CLI::App app{"Approximates the best possible approximation set for the input dataset using MPI."};
    AppData appData;
    Orchestrator::addMpiCmdOptions(app, appData);
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
    NaiveData baseData(*dataLoader);
    LocalData data(baseData, rowToRank, appData.worldRank);
    inputFile.close();

    delete dataLoader;

    Timers timers;
    timers.totalCalculationTime.startTimer();
    std::unique_ptr<RepresentativeSubsetCalculator> calculator(Orchestrator::getCalculator(appData));
    NaiveRepresentativeSubset localSolution(move(calculator), data, appData.outputSetSize, timers);

    // TODO: batch this into blocks using a custom MPI type to send higher volumes of data.
    std::vector<double> sendBuffer;
    unsigned int sendDataSize = BufferBuilder::buildSendBuffer(data, localSolution, sendBuffer);
    std::vector<int> receivingDataSizesBuffer(appData.worldSize, 0);
    MPI_Gather(&sendDataSize, 1, MPI_INT, receivingDataSizesBuffer.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    std::vector<double> receiveBuffer;
    std::vector<int> displacements;
    if (appData.worldRank == 0) {
        BufferBuilder::buildReceiveBuffer(receivingDataSizesBuffer, receiveBuffer);
        BufferBuilder::buildDisplacementBuffer(receivingDataSizesBuffer, displacements);
    }

    timers.communicationTime.startTimer();
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
    timers.communicationTime.stopTimer();

    if (appData.worldRank == 0) {
        std::unique_ptr<RepresentativeSubsetCalculator> globalCalculator(Orchestrator::getCalculator(appData));
        GlobalBufferLoader bufferLoader(receiveBuffer, data.totalColumns(), displacements, timers);
        std::unique_ptr<RepresentativeSubset> globalSolution(bufferLoader.getSolution(move(globalCalculator), appData.outputSetSize));

        timers.totalCalculationTime.stopTimer();

        nlohmann::json result = Orchestrator::buildMpiOutput(appData, *globalSolution.get(), data, timers);
        std::ofstream outputFile;
        outputFile.open(appData.outputFile);
        outputFile << result.dump(2);
        outputFile.close();
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}

   static double getTotalCoverage(const std::vector<std::pair<size_t, double>> &solution) {
        double totalCoverage = 0;
        for (const auto & s : solution) {
            totalCoverage += s.second;
        }

        return totalCoverage;
    }