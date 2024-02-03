#include <vector>
#include <cstdlib>
#include "bufferBuilder.h"

static const DummyRepresentativeSubset mockSolution(std::vector<size_t>{0, DATA.size()-1}, 15);

TEST_CASE("Testing the get total send data method") {
    std::vector<double> sendBuffer;
    unsigned int totalSendData = BufferBuilder::buildSendBuffer(data, mockSolution, sendBuffer);
    CHECK(totalSendData == (data.totalColumns() + 1) * mockSolution.getNumberOfRows() + 1);
    CHECK(sendBuffer.size() == totalSendData);
}

TEST_CASE("Test building send buffer") {
    std::vector<double> sendBuffer;
    unsigned int totalSendData = BufferBuilder::buildSendBuffer(data, mockSolution, sendBuffer);

    CHECK(sendBuffer.size() == totalSendData);
    std::vector<size_t> mockSolutionRows = mockSolution.getRows();
    for (size_t i = 0; i < mockSolution.getNumberOfRows(); i++) {
        size_t sentRow = static_cast<size_t>(sendBuffer[i * (data.totalColumns() + 1) + 1]);
        CHECK(sentRow == mockSolutionRows[i]);
    }
}

TEST_CASE("Getting solution from a buffer") {
    std::vector<double> sendBuffer;
    unsigned int totalSendData = BufferBuilder::buildSendBuffer(data, mockSolution, sendBuffer);
    
    std::vector<int> displacements;
    displacements.push_back(0);

    Timers timers;
    GlobalBufferLoader bufferLoader(sendBuffer, data.totalColumns(), displacements, timers);
    std::unique_ptr<RepresentativeSubset> receivedSolution(bufferLoader.getSolution(
        std::unique_ptr<RepresentativeSubsetCalculator>(new LazyRepresentativeSubsetCalculator()), 
        mockSolution.getNumberOfRows()
    ));

    CHECK(receivedSolution->getNumberOfRows() == mockSolution.getNumberOfRows());
    std::vector<size_t> mockSolutionRows = mockSolution.getRows();
    std::vector<size_t> receivedSolutionRows = receivedSolution->getRows();
    for (size_t i = 0; i < mockSolution.getNumberOfRows(); i++) {
        CHECK(receivedSolutionRows[i] == mockSolutionRows[i]);
    }
}