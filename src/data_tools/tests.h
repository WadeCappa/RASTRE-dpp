#include "data_row_visitor.h"
#include "to_binary_visitor.h"
#include "dot_product_visitor.h"
#include "data_row.h"
#include "data_row_factory.h"
#include "../representative_subset_calculator/representative_subset.h"
#include "base_data.h"

#include <utility>
#include <functional>
#include <doctest/doctest.h>

class DataRowVerifier : public DataRowVisitor {
    private:
    const size_t expectedRow;

    public:
    DataRowVerifier(size_t expectedRow) : expectedRow(expectedRow) {}

    void visitDenseDataRow(const std::vector<float>& data) {
        CHECK(data == DENSE_DATA[this->expectedRow]);
    }

    void visitSparseDataRow(const std::map<size_t, float>& data, size_t totalColumns) {
        CHECK(data == SPARSE_DATA_AS_MAP[expectedRow]);
        CHECK(totalColumns == SPARSE_DATA_TOTAL_COLUMNS);
    }
};

static std::string rowToString(const std::vector<float> &row) {
    std::ostringstream outputStream;
    for (const auto & v : row) {
        outputStream << v << ",";
    }
    std::string output = outputStream.str();
    output.pop_back();
    return output;
}

static std::string matrixToString(const std::vector<std::vector<float>> &data) {
    std::string output = "";
    for (const auto & row : data) {
        output += rowToString(row);
        output.append("\n");
    }

    return output;
}

static std::vector<std::unique_ptr<DataRow>> loadData(DataRowFactory &factory, std::istream &data) {
    std::vector<std::unique_ptr<DataRow>> res;
    FromFileLineFactory getter(data);
    std::unique_ptr<DataRow> nextRow(factory.maybeGet(getter));
    while (nextRow != nullptr) {
        res.push_back(move(nextRow));
        nextRow = factory.maybeGet(getter);
    }

    return move(res);
}

static void verifyData(const DataRow& row, const size_t expectedRow) {
    DataRowVerifier visitor(expectedRow);
    row.voidVisit(visitor);
}

static void verifyData(std::vector<std::unique_ptr<DataRow>> loadedData) {
    for (size_t row = 0; row < loadedData.size(); row++) {
        verifyData(*loadedData[row].get(), row);
    }
}

static void verifyData(const BaseData& data) {
    for (size_t i = 0; i < data.totalRows(); i++) {
        verifyData(data.getRow(i), i);
    }
}

static void verifyData(
    const SegmentedData& data,
    const std::vector<unsigned int> rankMapping,
    const unsigned int rank
) {
    size_t seen = 0;
    for (size_t i = 0; i < rankMapping.size(); i++) {
        if (rankMapping[i] == rank) {
            verifyData(data.getRow(seen), i);
            CHECK(data.getRemoteIndexForRow(seen) == i);
            seen++;
        }
    }
}

static void DEBUG_printData(const std::vector<std::vector<float>> &data) {
    for (const auto & d : data) {
        for (const auto & v : d) {
            std::cout << v << ", ";
        }
        std::cout << std::endl;
    }
}

TEST_CASE("Testing loading dense data") {
    std::string dataAsString = matrixToString(DENSE_DATA);
    std::istringstream inputStream(dataAsString);
    DenseDataRowFactory denseFactory;

    auto data = loadData(denseFactory, inputStream);

    CHECK(data.size() == DENSE_DATA.size());
    verifyData(move(data));
}

TEST_CASE("Testing loading sparse data") {
    std::string dataAsString = matrixToString(SPARSE_DATA);
    std::istringstream inputStream(dataAsString);
    SparseDataRowFactory sparseFactory(SPARSE_DATA_TOTAL_COLUMNS);

    auto data = loadData(sparseFactory, inputStream);

    CHECK(data.size() == SPARSE_DATA_AS_MAP.size());
    verifyData(move(data));
}

TEST_CASE("Testing dense base data") {
    std::string dataAsString = matrixToString(DENSE_DATA);
    std::istringstream inputStream(dataAsString);
    FromFileLineFactory getter(inputStream);
    DenseDataRowFactory factory;
    std::unique_ptr<FullyLoadedData> data(FullyLoadedData::load(factory, getter));
    verifyData(*data.get());
}

TEST_CASE("Testing sparse base data") {
    std::string dataAsString = matrixToString(SPARSE_DATA);
    std::istringstream inputStream(dataAsString);
    FromFileLineFactory getter(inputStream);
    SparseDataRowFactory factory(SPARSE_DATA_TOTAL_COLUMNS);
    std::unique_ptr<FullyLoadedData> data(FullyLoadedData::load(factory, getter));
    verifyData(*data.get());
}

TEST_CASE("Testing segmented dense data") {
    std::string dataAsString = matrixToString(SPARSE_DATA);
    std::istringstream inputStream(dataAsString);
    FromFileLineFactory getter(inputStream);
    SparseDataRowFactory factory(SPARSE_DATA_TOTAL_COLUMNS);
    
    std::vector<unsigned int> rankMapping({0, 1, 2, 3, 2, 1});
    const int rank = 2;

    std::unique_ptr<SegmentedData> data(SegmentedData::load(factory, getter, rankMapping, rank));
    CHECK(data->totalRows() == 2);
    verifyData(*data.get(), rankMapping, rank);
}

TEST_CASE("Testing ReceivedData translation and construction") {
    std::unique_ptr<std::vector<std::pair<size_t, std::unique_ptr<DataRow>>>> mockReceiveData(
        new std::vector<std::pair<size_t, std::unique_ptr<DataRow>>>()
    );

    std::vector<size_t> mockSolutionIndicies;
    mockSolutionIndicies.push_back(1);
    mockSolutionIndicies.push_back(DENSE_DATA.size() - 1);
    for (const auto & i : mockSolutionIndicies) {
        mockReceiveData->push_back(
            std::make_pair(
                i, 
                std::unique_ptr<DataRow>(new DenseDataRow(DENSE_DATA[i]))
            )
        );
    }

    ReceivedData data(move(mockReceiveData));

    CHECK(data.totalRows() == mockSolutionIndicies.size());
    CHECK(data.totalColumns() == DENSE_DATA[0].size());

    for (size_t i = 0; i < data.totalRows(); i++) {
        verifyData(data.getRow(i), mockSolutionIndicies[i]);
    }

    std::unique_ptr<MutableSubset> mockSolution(NaiveMutableSubset::makeNew());

    for (size_t i = 0; i < data.totalRows(); i++) {
        mockSolution->addRow(i, 0);
    }

    auto translated = data.translateSolution(MutableSubset::upcast(move(mockSolution)));
    CHECK(mockSolutionIndicies.size() == translated->size());

    int i = 0;
    for (const auto v : *translated.get()) {
        CHECK(mockSolutionIndicies[i++] == v);
    }
}

TEST_CASE("Testing dense binary to data row conversion") {
    for (size_t i = 0; i < DENSE_DATA.size(); i++) {
        // Test binary to data row
        DenseDataRowFactory factory;
        std::unique_ptr<DataRow> row(factory.getFromBinary(DENSE_DATA[i]));
        DataRowVerifier visitor(i);
        row->voidVisit(visitor);
    
        // Test data row to binary
        ToBinaryVisitor toBinary;
        std::vector<float> newBinary(row->visit(toBinary));
        
        // Verify old and new binaries are equivalent
        CHECK(newBinary == DENSE_DATA[i]);
    }
}

TEST_CASE("Testing sparse binary to data row conversion") {
    for (size_t i = 0; i < SPARSE_DATA_AS_MAP.size(); i++) {
        std::unique_ptr<DataRow> row(new SparseDataRow(SPARSE_DATA_AS_MAP[i], SPARSE_DATA_TOTAL_COLUMNS));
    
        // Test data row to binary
        ToBinaryVisitor toBinary;
        std::vector<float> newBinary(row->visit(toBinary));
    
        // Try loading from binary
        SparseDataRowFactory factory(SPARSE_DATA_TOTAL_COLUMNS);
        std::unique_ptr<DataRow> fromBinaryRow(factory.getFromBinary(move(newBinary)));
        DataRowVerifier visitor(i);
        fromBinaryRow->voidVisit(visitor);
    }
}

TEST_CASE("Testing dot products") {
    for (size_t s = 0; s < SPARSE_DATA_AS_MAP.size(); s++) {
        for (size_t d = 0; d < DENSE_DATA.size(); d++) {
            std::unique_ptr<DataRow> denseRow(new DenseDataRow(DENSE_DATA[d]));
            std::unique_ptr<DataRow> sparseRow(new SparseDataRow(SPARSE_DATA_AS_MAP[s], SPARSE_DATA_TOTAL_COLUMNS));

            // dense to dense
            float denseToDense = denseRow->dotProduct(*denseRow);
            CHECK(denseToDense > 0);
            
            // dense to sparse
            float denseToSparse = denseRow->dotProduct(*sparseRow);
            CHECK(denseToSparse > 0);

            // sparse to dense
            float sparseToDense = sparseRow->dotProduct(*denseRow);
            CHECK(sparseToDense > 0);

            // sparse to sparse
            float sparseToSparse = sparseRow->dotProduct(*sparseRow);
            CHECK(sparseToSparse > 0);
        }
    }
}