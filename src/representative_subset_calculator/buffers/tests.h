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

TEST_CASE("Test to binary and back to matrix") {
    std::vector<double> sendBuffer;
    unsigned int totalSendData = BufferBuilder::buildSendBuffer(data, mockSolution, sendBuffer);
    
    std::vector<int> displacements;
    displacements.push_back(0);

    BufferLoader bufferLoader(sendBuffer, data.totalColumns(), displacements);
    std::vector<std::pair<size_t, std::vector<double>>> newData = *bufferLoader.returnNewData().get();
    CHECK(newData.size() == mockSolution.getNumberOfRows());

    std::vector<size_t> mockSolutionRows = mockSolution.getRows();
    for (size_t i = 0; i < mockSolution.getNumberOfRows(); i++) {
        CHECK(newData[i].first == mockSolutionRows[i]);
        CHECK(newData[i].second == data.getRow(mockSolutionRows[i]));
    }
}

TEST_CASE("Mock MPI test sending and receiving a buffer") {
    std::vector<double> sendBufferRank0;
    std::vector<double> sendBufferRank1;
    unsigned int totalSendDataRank0 = BufferBuilder::buildSendBuffer(data, mockSolution, sendBufferRank0);
    unsigned int totalSendDataRank1 = BufferBuilder::buildSendBuffer(data, mockSolution, sendBufferRank1);

    std::vector<int> displacements;
    displacements.push_back(0);
    displacements.push_back(totalSendDataRank0);

    for (const auto & d : sendBufferRank1) {
        sendBufferRank0.push_back(d);
    }

    BufferLoader bufferLoader(sendBufferRank0, data.totalColumns(), displacements);
    std::vector<std::pair<size_t, std::vector<double>>> newData = *bufferLoader.returnNewData().get();
    CHECK(newData.size() == mockSolution.getNumberOfRows() + mockSolution.getNumberOfRows());

    std::vector<size_t> mockSolutionRows = mockSolution.getRows();
    for (size_t i = 0; i < mockSolution.getNumberOfRows(); i++) {
        CHECK(newData[i].first == mockSolutionRows[i]);
        CHECK(newData[i].second == data.getRow(mockSolutionRows[i]));
    }

    for (size_t i = 0; i < mockSolution.getNumberOfRows(); i++) {
        CHECK(newData[i + mockSolution.getNumberOfRows()].first == mockSolutionRows[i]);
        CHECK(newData[i + mockSolution.getNumberOfRows()].second == data.getRow(mockSolutionRows[i]));
    }
}