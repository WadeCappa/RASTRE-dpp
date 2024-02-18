#include <vector>
#include <cstdlib>
#include "bufferBuilder.h"

static const std::unique_ptr<RepresentativeSubset> MOCK_SOLUTION(
    RepresentativeSubset::of(std::vector<size_t>{0, DATA.size()-1}, 15)
);
static const std::vector<unsigned int> ROW_TO_RANK(DATA.size(), 0);
static const unsigned int RANK = 0;

static std::vector<size_t> getRows(const RepresentativeSubset &solution) {
    std::vector<size_t> rows;
    for (size_t i = 0; i < solution.size(); i++) {
        rows.push_back(solution.getRow(i));
    }

    return rows;
}

TEST_CASE("Testing the get total send data method") {
    std::vector<double> sendBuffer;
    LocalData localData(data, ROW_TO_RANK, RANK);
    unsigned int totalSendData = BufferBuilder::buildSendBuffer(localData, *MOCK_SOLUTION.get(), sendBuffer);
    CHECK(totalSendData == (data.totalColumns() + 1) * MOCK_SOLUTION->size() + 1);
    CHECK(sendBuffer.size() == totalSendData);
}

TEST_CASE("Test building send buffer") {
    std::vector<double> sendBuffer;
    unsigned int totalSendData = BufferBuilder::buildSendBuffer(LocalData(data, ROW_TO_RANK, RANK), *MOCK_SOLUTION.get(), sendBuffer);

    CHECK(sendBuffer.size() == totalSendData);
    std::vector<size_t> mockSolutionRows = getRows(*MOCK_SOLUTION.get());
    for (size_t i = 0; i < MOCK_SOLUTION->size(); i++) {
        size_t sentRow = static_cast<size_t>(sendBuffer[i * (data.totalColumns() + 1) + 1]);
        CHECK(sentRow == mockSolutionRows[i]);
    }
}

TEST_CASE("Getting solution from a buffer") {
    std::vector<double> sendBuffer;
    unsigned int totalSendData = BufferBuilder::buildSendBuffer(LocalData(data, ROW_TO_RANK, RANK), *MOCK_SOLUTION.get(), sendBuffer);
    
    std::vector<int> displacements;
    displacements.push_back(0);

    Timers timers;
    GlobalBufferLoader bufferLoader(sendBuffer, data.totalColumns(), displacements, timers);
    std::unique_ptr<RepresentativeSubset> receivedSolution(bufferLoader.getSolution(
        std::unique_ptr<RepresentativeSubsetCalculator>(new NaiveRepresentativeSubsetCalculator()), 
        MOCK_SOLUTION->size()
    ));

    CHECK(receivedSolution->size() == MOCK_SOLUTION->size());
    std::vector<size_t> mockSolutionRows = getRows(*MOCK_SOLUTION.get());
    std::vector<size_t> receivedSolutionRows = getRows(*receivedSolution.get());
    for (size_t i = 0; i < MOCK_SOLUTION->size(); i++) {
        CHECK(receivedSolutionRows[i] == mockSolutionRows[i]);
    }
}