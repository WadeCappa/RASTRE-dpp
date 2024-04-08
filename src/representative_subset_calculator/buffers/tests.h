#include <vector>
#include <cstdlib>
#include "buffer_builder_visitor.h"
#include "bufferBuilder.h"

static const std::unique_ptr<Subset> MOCK_SOLUTION(
    Subset::of(std::vector<size_t>{0, DENSE_DATA.size()-1}, 15)
);
static const std::vector<unsigned int> ROW_TO_RANK(DENSE_DATA.size(), 0);
static const unsigned int RANK = 0;

static std::vector<size_t> getRows(const Subset &solution) {
    std::vector<size_t> rows;
    for (size_t i = 0; i < solution.size(); i++) {
        rows.push_back(solution.getRow(i));
    }

    return rows;
}

static std::vector<std::unique_ptr<DataRow>> getRawDenseData() {
    std::vector<std::unique_ptr<DataRow>> res;
    for (const auto & d : DENSE_DATA) {
        res.push_back(std::unique_ptr<DataRow>(new DenseDataRow(d)));
    }

    return move(res);
};

static std::unique_ptr<SegmentedData> getData() {
    std::vector<size_t> localRowToGlobalRow;
    for (size_t i = 0; i < DENSE_DATA.size(); i++) {
        localRowToGlobalRow.push_back(i);
    }

    return std::unique_ptr<SegmentedData>(
        new SegmentedData(
            move(getRawDenseData()), 
            localRowToGlobalRow, 
            DENSE_DATA[0].size()
        )
    );
}

TEST_CASE("Testing the get total send data method") {
    std::unique_ptr<SegmentedData> data(getData());

    std::vector<double> sendBuffer;
    unsigned int totalSendData = BufferBuilder::buildSendBuffer(*data, *MOCK_SOLUTION.get(), sendBuffer);
    CHECK(totalSendData == (data->totalColumns() + 1) * MOCK_SOLUTION->size() + 1);
    CHECK(sendBuffer.size() == totalSendData);
}

TEST_CASE("Test building send buffer") {
    std::unique_ptr<SegmentedData> data(getData());

    std::vector<double> sendBuffer;
    unsigned int totalSendData = BufferBuilder::buildSendBuffer(*data, *MOCK_SOLUTION.get(), sendBuffer);

    CHECK(sendBuffer.size() == totalSendData);
    CHECK(sendBuffer.size() > 0);
    std::vector<size_t> mockSolutionRows = getRows(*MOCK_SOLUTION.get());
    for (size_t i = 0; i < MOCK_SOLUTION->size(); i++) {
        size_t sentRow = static_cast<size_t>(sendBuffer[i * (data->totalColumns() + 1) + 1]);
        CHECK(sentRow == mockSolutionRows[i]);
    }
}

TEST_CASE("Getting solution from a buffer") {
    std::unique_ptr<SegmentedData> data(getData());

    std::vector<double> sendBuffer;
    unsigned int totalSendData = BufferBuilder::buildSendBuffer(*data, *MOCK_SOLUTION.get(), sendBuffer);
    
    std::vector<int> displacements;
    displacements.push_back(0);

    Timers timers;
    GlobalBufferLoader bufferLoader(sendBuffer, data->totalColumns(), displacements, timers);
    std::unique_ptr<Subset> receivedSolution(bufferLoader.getSolution(
        std::unique_ptr<SubsetCalculator>(new NaiveSubsetCalculator()), 
        MOCK_SOLUTION->size()
    ));

    CHECK(receivedSolution->size() == MOCK_SOLUTION->size());
    std::vector<size_t> mockSolutionRows = getRows(*MOCK_SOLUTION.get());
    std::vector<size_t> receivedSolutionRows = getRows(*receivedSolution.get());
    for (size_t i = 0; i < MOCK_SOLUTION->size(); i++) {
        CHECK(receivedSolutionRows[i] == mockSolutionRows[i]);
    }
}