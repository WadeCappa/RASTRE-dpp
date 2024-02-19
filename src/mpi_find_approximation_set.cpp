#include "representative_subset_calculator/representative_subset.h"
#include "data_tools/normalizer.h"
#include "data_tools/matrix_builder.h"
#include "representative_subset_calculator/timers/timers.h"
#include "representative_subset_calculator/naive_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_representative_subset_calculator.h"
#include "representative_subset_calculator/fast_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_fast_representative_subset_calculator.h"
#include "representative_subset_calculator/orchestrator/mpi_orchestrator.h"

#include "representative_subset_calculator/buffers/bufferBuilder.h"
#include "representative_subset_calculator/streaming/streaming.h"

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

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

    std::vector<unsigned int> rowToRank = MpiOrchestrator::getRowToRank(appData, seed);

    Timers timers;
    timers.loadingDatasetTime.startTimer();
    std::ifstream inputFile;
    inputFile.open(appData.inputFile);
    DataLoader *dataLoader = MpiOrchestrator::buildMpiDataLoader(appData, inputFile, rowToRank);
    NaiveData baseData(*dataLoader);
    LocalData data(baseData, rowToRank, appData.worldRank);
    inputFile.close();
    timers.loadingDatasetTime.stopTimer();

    delete dataLoader;

    timers.barrierTime.startTimer();
    MPI_Barrier(MPI_COMM_WORLD);
    timers.barrierTime.stopTimer();

    timers.totalCalculationTime.startTimer();
    std::unique_ptr<SubsetCalculator> calculator(MpiOrchestrator::getCalculator(appData));

    timers.localCalculationTime.startTimer();
    std::unique_ptr<Subset> localSolution(calculator->getApproximationSet(data, appData.outputSetSize));
    timers.localCalculationTime.stopTimer();

    // TODO: batch this into blocks using a custom MPI type to send higher volumes of data.
    timers.bufferEncodingTime.startTimer();
    std::vector<double> sendBuffer;
    unsigned int sendDataSize = BufferBuilder::buildSendBuffer(data, *localSolution.get(), sendBuffer);
    std::vector<int> receivingDataSizesBuffer(appData.worldSize, 0);
    timers.bufferEncodingTime.stopTimer();

    timers.communicationTime.startTimer();
    MPI_Gather(&sendDataSize, 1, MPI_INT, receivingDataSizesBuffer.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
    timers.communicationTime.stopTimer();

    timers.bufferEncodingTime.startTimer();
    std::vector<double> receiveBuffer;
    std::vector<int> displacements;
    if (appData.worldRank == 0) {
        BufferBuilder::buildReceiveBuffer(receivingDataSizesBuffer, receiveBuffer);
        BufferBuilder::buildDisplacementBuffer(receivingDataSizesBuffer, displacements);
    }
    timers.bufferEncodingTime.stopTimer();

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
        std::unique_ptr<SubsetCalculator> globalCalculator(MpiOrchestrator::getCalculator(appData));
        GlobalBufferLoader bufferLoader(receiveBuffer, data.totalColumns(), displacements, timers);
        std::unique_ptr<Subset> globalSolution(bufferLoader.getSolution(move(globalCalculator), appData.outputSetSize));

        timers.totalCalculationTime.stopTimer();

        nlohmann::json result = MpiOrchestrator::buildMpiOutput(appData, *globalSolution.get(), data, timers, rowToRank);
        std::ofstream outputFile;
        outputFile.open(appData.outputFile);
        outputFile << result.dump(2);
        outputFile.close();
    } else {
        // used to load global timers on rank 0
        timers.totalCalculationTime.stopTimer();
        auto dummyResult = Subset::empty();

        MpiOrchestrator::buildMpiOutput(appData, *dummyResult.get(), data, timers, rowToRank);
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}