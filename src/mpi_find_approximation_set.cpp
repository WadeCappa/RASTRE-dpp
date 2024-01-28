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

static unsigned int getTotalSendData(
    const Data &data, 
    const std::vector<std::pair<size_t, double>> &localSolution
) {
    return data.totalColumns() * localSolution.size();
}

static void buildSendBuffer(
    const Data &data, 
    const std::vector<std::pair<size_t, double>> &localSolution, 
    std::vector<double> &buffer,
    const unsigned int totalData 
) {
    size_t rowSize = data.totalColumns();
    size_t numberOfRows = localSolution.size();
    buffer.resize(totalData);

    #pragma parallel for
    for (size_t rowIndex = 0; rowIndex < numberOfRows; rowIndex++) {
        const auto & row = data.getRow(localSolution[rowIndex].first);
        for (size_t columnIndex = 0; columnIndex < rowSize; columnIndex++) {
            double v = row[columnIndex];
            buffer[rowSize * rowIndex + columnIndex] = v;
        }
    }
}

static void buildReceiveBuffer(
    const std::vector<int> &sendSizes, 
    std::vector<double> &receiveBuffer
) {
    size_t totalData = 0;
    for (const auto & d : sendSizes) {
        totalData += d;
    }

    receiveBuffer.resize(totalData);
}

static std::vector<int> buildDisplacementBuffer(const std::vector<int> &sendSizes) {
    std::vector<int> res;
    unsigned int seenData = 0;
    for (const auto & s : sendSizes) {
        res.push_back(seenData);
        seenData += s;
    }

    return res;
}

static void rebuildData(
    const std::vector<double> &binaryInput, 
    const size_t rowSize,
    std::vector<std::vector<double>> &newData
) {
    const size_t expectedRows = std::floor(binaryInput.size() / rowSize);
    if (binaryInput.size() % rowSize != 0) {
        std::cout << "ERROR: did not get an expected number of values from all gather" << std::endl;
    }

    newData.clear();
    newData.resize(expectedRows);
    
    #pragma omp parallel for
    for (size_t currentRow = 0; currentRow < expectedRows; currentRow++) {
        newData[currentRow] = std::vector<double>(binaryInput.begin() + (rowSize * currentRow), binaryInput.begin() + (rowSize * (currentRow + 1)));
    }
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
    std::vector<std::pair<size_t, double>> localSolution = calculator->getApproximationSet(data, appData.outputSetSize);

    // TODO: batch this into blocks using a custom MPI type to send higher volumes of data.
    unsigned int sendDataSize = getTotalSendData(data, localSolution);
    std::vector<int> receivingDataSizesBuffer(appData.worldSize, 0);
    MPI_Gather(&sendDataSize, 1, MPI_INT, receivingDataSizesBuffer.data(), appData.worldSize, MPI_INT, 0, MPI_COMM_WORLD);

    std::vector<double> sendBuffer;
    buildSendBuffer(data, localSolution, sendBuffer, sendDataSize);

    std::vector<double> receiveBuffer;
    buildReceiveBuffer(receivingDataSizesBuffer, receiveBuffer);
    std::vector<int> displacements = buildDisplacementBuffer(receivingDataSizesBuffer);
    MPI_Gatherv(
        sendBuffer.data(), 
        sendDataSize, 
        MPI_DOUBLE, 
        receiveBuffer.data(), 
        receivingDataSizesBuffer.data(), 
        displacements.data(),
        MPI_DOUBLE, 
        0, 
        MPI_COMM_WORLD
    );

    if (appData.worldRank == 0) {
        std::vector<std::vector<double>> newData;
        rebuildData(receiveBuffer, data.totalColumns(), newData);
        NaiveData bestRows(newData, newData.size(), data.totalColumns());

        std::vector<std::pair<size_t, double>> solution = calculator->getApproximationSet(bestRows, appData.outputSetSize);

        nlohmann::json result = Orchestrator::buildOutput(appData, solution, data, timers);

        std::ofstream outputFile;
        outputFile.open(appData.outputFile);
        outputFile << result.dump(2);
        outputFile.close();
    }

    return 0;
}