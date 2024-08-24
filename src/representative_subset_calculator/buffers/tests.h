#include <vector>
#include <cstdlib>
#include "buffer_builder.h"

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

static std::vector<std::unique_ptr<DataRow>> getRawSparseData() {
    std::vector<std::unique_ptr<DataRow>> res;
    auto map(toMap());
    for (const auto & d : map) {
        res.push_back(std::unique_ptr<DataRow>(new SparseDataRow(d, SPARSE_DATA_TOTAL_COLUMNS)));
    }

    return move(res);
};

static std::unique_ptr<SegmentedData> getData(
    std::vector<std::unique_ptr<DataRow>> base, 
    size_t rows,
    size_t columns
) {
    std::vector<size_t> localRowToGlobalRow;
    for (size_t i = 0; i < rows; i++) {
        localRowToGlobalRow.push_back(i);
    }

    return std::unique_ptr<SegmentedData>(
        new SegmentedData(move(base), localRowToGlobalRow, columns)
    );
};

static std::unique_ptr<SegmentedData> getDenseData() {
    std::vector<std::unique_ptr<DataRow>> data(getRawDenseData());
    size_t rows = data.size();
    size_t columns = data[0]->size();
    return getData(move(data), rows, columns);
}

static std::unique_ptr<SegmentedData> getSparseData() {
    std::vector<std::unique_ptr<DataRow>> data(getRawSparseData());
    size_t rows = data.size();
    size_t columns = data[0]->size();
    return getData(move(data), rows, columns);
}

TEST_CASE("Testing the get total send dense data method") {
    std::unique_ptr<SegmentedData> denseData(getDenseData());

    std::vector<float> sendBuffer;
    unsigned int totalSendData = BufferBuilder::buildSendBuffer(*denseData, *MOCK_SOLUTION.get(), sendBuffer);
    CHECK(totalSendData == (denseData->totalColumns() + 2) * MOCK_SOLUTION->size() + 1);
    CHECK(sendBuffer.size() == totalSendData);
}

TEST_CASE("Testing the get total send sparse data method") {
    std::unique_ptr<SegmentedData> sparseData(getSparseData());

    std::vector<float> sendBuffer;
    unsigned int totalSendData = BufferBuilder::buildSendBuffer(*sparseData, *MOCK_SOLUTION.get(), sendBuffer);
    CHECK(totalSendData == (sparseData->totalColumns() + 1) * MOCK_SOLUTION->size() + 1);
    CHECK(sendBuffer.size() == totalSendData);
}

TEST_CASE("Test building send buffers for") {
    std::unique_ptr<SegmentedData> data(getDenseData());

    std::vector<float> sendBuffer;
    unsigned int totalSendData = BufferBuilder::buildSendBuffer(*data, *MOCK_SOLUTION.get(), sendBuffer);

    CHECK(sendBuffer.size() == totalSendData);
    CHECK(sendBuffer.size() > 0);

    std::vector<size_t> mockSolutionRows = getRows(*MOCK_SOLUTION.get());
    for (size_t i = 0; i < MOCK_SOLUTION->size(); i++) {
        size_t sentRow = static_cast<size_t>(sendBuffer[((i + 1) * (data->totalColumns() + 2)) - 1 ]);
        CHECK(sentRow == mockSolutionRows[i]);
    }
}

TEST_CASE("Getting solution from a buffer") {
    std::unique_ptr<SegmentedData> denseData(getDenseData());

    std::vector<float> sendBuffer;
    unsigned int totalSendData = BufferBuilder::buildSendBuffer(*denseData, *MOCK_SOLUTION.get(), sendBuffer);
    
    std::vector<int> displacements;
    displacements.push_back(0);

    Timers timers;
    GlobalBufferLoader bufferLoader(sendBuffer, denseData->totalColumns(), displacements, timers);
    std::unique_ptr<Subset> receivedSolution(
        bufferLoader.getSolution(
            std::unique_ptr<SubsetCalculator>(new NaiveSubsetCalculator()), 
            MOCK_SOLUTION->size(),
            DenseDataRowFactory()
        )
    );

    CHECK(receivedSolution->size() == MOCK_SOLUTION->size());
    std::vector<size_t> mockSolutionRows = getRows(*MOCK_SOLUTION.get());
    std::vector<size_t> receivedSolutionRows = getRows(*receivedSolution.get());
    for (size_t i = 0; i < MOCK_SOLUTION->size(); i++) {
        CHECK(receivedSolutionRows[i] == mockSolutionRows[i]);
    }
}