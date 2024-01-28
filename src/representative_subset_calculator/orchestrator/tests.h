#include <vector>
#include <cstdlib>
#include "mpi_orchestrator.h"

void validateValidRanks(const std::vector<unsigned int> &allRanks, const unsigned int worldSize) {
    for (const auto & rank : allRanks) {
        CHECK(rank >= 0);
        CHECK(rank < worldSize);
    }
}

std::vector<std::pair<size_t, double>> getMockSolution() {
    std::vector<std::pair<size_t, double>> mockSolution;
    mockSolution.push_back(std::make_pair(0, 0));
    mockSolution.push_back(std::make_pair(DATA.size() - 1, 0));
    return mockSolution;
}

TEST_CASE("Testing the proper blocking of a dataset with an evenly divisible dataset") {
    const unsigned int WORLD_SIZE = DATA.size();

    AppData appData;
    appData.numberOfDataRows = DATA.size();
    appData.worldSize = WORLD_SIZE;
    
    std::vector<unsigned int> previous;
    std::srand((unsigned) time(NULL));
    int seed = std::rand();

    for (unsigned int rank = 0; rank < WORLD_SIZE; rank++) {
        appData.worldRank = rank;
        auto allRanks = Orchestrator::getRowToRank(appData, seed);
        if (previous.size() == 0) {
            previous = allRanks;
        }
        CHECK(previous == allRanks);
        CHECK(allRanks.size() == appData.numberOfDataRows);
        validateValidRanks(allRanks, appData.worldSize);
    }
}

TEST_CASE("Testing the get total send data method") {
    auto mockSolution = getMockSolution();
    unsigned int totalSendData = MpiOrchestrator::getTotalSendData(data, mockSolution);
    CHECK(totalSendData == (data.totalColumns() + 1) * mockSolution.size());
}

TEST_CASE("Test building send buffer") {
    auto mockSolution = getMockSolution();
    unsigned int sendData = MpiOrchestrator::getTotalSendData(data, mockSolution);

    std::vector<double> sendBuffer;
    MpiOrchestrator::buildSendBuffer(data, mockSolution, sendBuffer, sendData);

    CHECK(sendBuffer.size() == sendData);
    for (size_t i = 0; i < mockSolution.size(); i++) {
        size_t sentRow = static_cast<size_t>(sendBuffer[i * (data.totalColumns() + 1)]);
        CHECK(sentRow == mockSolution[i].first);
    }
}

TEST_CASE("Test to binary and back to matrix") {
    auto mockSolution = getMockSolution();
    unsigned int totalData = MpiOrchestrator::getTotalSendData(data, mockSolution);
    std::vector<double> sendBuffer;
    MpiOrchestrator::buildSendBuffer(data, mockSolution, sendBuffer, totalData);

    std::vector<std::pair<size_t, std::vector<double>>> newData;
    MpiOrchestrator::rebuildData(sendBuffer, data.totalColumns(), newData);
    for (size_t i = 0; i < mockSolution.size(); i++) {
        CHECK(newData[i].second == data.getRow(mockSolution[i].first));
        CHECK(newData[i].first == mockSolution[i].first);
    }

    CHECK(newData.size() == mockSolution.size());
}