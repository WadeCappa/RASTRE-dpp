#include <vector>
#include <cstdlib>
#include "bufferBuilder.h"

static const DummyRepresentativeSubset MOCK_SOLUTION(std::vector<size_t>{0, DATA.size()-1}, 15);
static const std::vector<unsigned int> ROW_TO_RANK(DATA.size(), 0);
static const unsigned int RANK = 0;

TEST_CASE("Testing the get total send data method") {
    std::vector<double> sendBuffer;
    unsigned int totalSendData = BufferBuilder::buildSendBuffer(LocalData(data, ROW_TO_RANK, RANK), MOCK_SOLUTION, sendBuffer);
    CHECK(totalSendData == (data.totalColumns() + 1) * MOCK_SOLUTION.getNumberOfRows() + 1);
    CHECK(sendBuffer.size() == totalSendData);
}

TEST_CASE("Test building send buffer") {
    std::vector<double> sendBuffer;
    unsigned int totalSendData = BufferBuilder::buildSendBuffer(LocalData(data, ROW_TO_RANK, RANK), MOCK_SOLUTION, sendBuffer);

    CHECK(sendBuffer.size() == totalSendData);
    std::vector<size_t> mockSolutionRows = MOCK_SOLUTION.getRows();
    for (size_t i = 0; i < MOCK_SOLUTION.getNumberOfRows(); i++) {
        size_t sentRow = static_cast<size_t>(sendBuffer[i * (data.totalColumns() + 1) + 1]);
        CHECK(sentRow == mockSolutionRows[i]);
    }
}

TEST_CASE("Getting solution from a buffer") {
    std::vector<double> sendBuffer;
    unsigned int totalSendData = BufferBuilder::buildSendBuffer(LocalData(data, ROW_TO_RANK, RANK), MOCK_SOLUTION, sendBuffer);
    
    std::vector<int> displacements;
    displacements.push_back(0);

    Timers timers;
    GlobalBufferLoader bufferLoader(sendBuffer, data.totalColumns(), displacements, timers);
    std::unique_ptr<RepresentativeSubset> receivedSolution(bufferLoader.getSolution(
        std::unique_ptr<RepresentativeSubsetCalculator>(new LazyRepresentativeSubsetCalculator()), 
        MOCK_SOLUTION.getNumberOfRows()
    ));

    CHECK(receivedSolution->getNumberOfRows() == MOCK_SOLUTION.getNumberOfRows());
    std::vector<size_t> mockSolutionRows = MOCK_SOLUTION.getRows();
    std::vector<size_t> receivedSolutionRows = receivedSolution->getRows();
    for (size_t i = 0; i < MOCK_SOLUTION.getNumberOfRows(); i++) {
        CHECK(receivedSolutionRows[i] == mockSolutionRows[i]);
    }
}