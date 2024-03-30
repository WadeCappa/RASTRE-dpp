#include "../representative_subset_calculator/representative_subset.h"
#include "data_row.h"
#include "normalizer.h"
#include "data_saver.h"
#include "matrix_builder.h"

#include <utility>
#include <functional>
#include <doctest/doctest.h>

static std::pair<std::vector<double>, double> VECTOR_WITH_LENGTH = std::make_pair(
    std::vector<double>{3.0, 3.0, 3.0, 3.0},
    6.0
);

static std::string rowToString(const std::vector<double> &row) {
    std::ostringstream outputStream;
    for (const auto & v : row) {
        outputStream << v << ",";
    }
    std::string output = outputStream.str();
    output.pop_back();
    return output;
}

static std::string matrixToString(const std::vector<std::vector<double>> &data) {
    std::string output = "";
    for (const auto & row : data) {
        output += rowToString(row);
        output.append("\n");
    }

    return output;
}

static void validateNormalizedVector(const std::vector<double> &data) {
    for (const auto & e : data) {
        CHECK(e <= 1.0);
    }

    double length = Normalizer::vectorLength(data);
    CHECK(length > 0.9);
    CHECK(length < 1.1);
}

static std::vector<std::vector<double>> loadData(DataLoader &loader) {
    std::vector<std::vector<double>> data;
    std::vector<double> element;
    while (loader.getNext(element)) {
        data.push_back(element);
    }

    return data;
}

static void DEBUG_printData(const std::vector<std::vector<double>> &data) {
    for (const auto & d : data) {
        for (const auto & v : d) {
            std::cout << v << ", ";
        }
        std::cout << std::endl;
    }
}

TEST_CASE("Testing loading data") {
    std::string dataAsString = matrixToString(DATA);
    std::istringstream inputStream(dataAsString);
    AsciiDataLoader loader(inputStream);

    auto data = loadData(loader);

    CHECK(data == DATA);
}

TEST_CASE("Testing length calculation") {
    double length = Normalizer::vectorLength(VECTOR_WITH_LENGTH.first);
    CHECK(length == VECTOR_WITH_LENGTH.second);
}

TEST_CASE("Testing vector normalization") {
    for (const auto & e : DATA) {
        std::vector<double> copy = e;
        Normalizer::normalize(copy);

        validateNormalizedVector(copy);
    }
}

TEST_CASE("Testing the normalized data loader") {
    std::istringstream inputStream(matrixToString(DATA));
    AsciiDataLoader dataLoader(inputStream);
    Normalizer normalizer(dataLoader);
    std::vector<double> element;
    while (normalizer.getNext(element)) {
        validateNormalizedVector(element);
    }
}

TEST_CASE("Testing saving data") {
    std::string dataAsString = matrixToString(DATA);
    std::ostringstream stringStream;

    std::istringstream inputStream(dataAsString);
    AsciiDataLoader dataLoader(inputStream);
    AsciiDataSaver saver(dataLoader);
    stringStream << saver;
    CHECK(stringStream.str() == dataAsString);
}

TEST_CASE("Testing loading and saving binary data") {
    std::string dataAsString = matrixToString(DATA);
    std::istringstream asciiStream(dataAsString);

    AsciiDataLoader asciiDataLoader(asciiStream);
    BinaryDataSaver saver(asciiDataLoader);

    std::ostringstream outputStream;
    outputStream << saver;

    std::istringstream binaryStream(outputStream.str());
    BinaryDataLoader binaryDataLoader(binaryStream);

    std::vector<std::vector<double>> data = loadData(binaryDataLoader);

    CHECK(data == DATA);
}

TEST_CASE("Testing matrix builder") {
    std::string dataAsString = matrixToString(DATA);
    std::istringstream inputStream(dataAsString);
    AsciiDataLoader loader(inputStream);

    NaiveData matrix(loader);

    CHECK(matrix.DEBUG_compareData(DATA));
    CHECK(matrix.totalRows() == DATA.size());
    CHECK(matrix.totalColumns() == DATA[0].size());
}

TEST_CASE("Testing blocked data loader") {
    std::istringstream inputStream(matrixToString(DATA));
    AsciiDataLoader dataLoader(inputStream);
    
    std::vector<std::vector<double>> ownedRows;
    ownedRows.push_back(DATA[0]);
    ownedRows.push_back(DATA[3]);

    const int RANK = 1;
    
    std::vector<unsigned int> ownership(DATA.size(), 0);
    ownership[0] = RANK;
    ownership[3] = RANK;

    BlockedDataLoader blockedDataLoader(dataLoader, ownership, RANK);
    std::vector<double> element;
    std::vector<std::vector<double>> rows;
    while (blockedDataLoader.getNext(element)) {
        rows.push_back(element);
    }

    CHECK(rows.size() == 2);
    CHECK(rows == ownedRows);
}

TEST_CASE("Testing SelectiveData translation and construction") {
    std::vector<std::pair<size_t, std::vector<double>>> mockReceiveData;
    std::vector<size_t> mockSolutionIndicies;
    mockSolutionIndicies.push_back(1);
    mockSolutionIndicies.push_back(DATA.size() - 1);
    for (const auto & i : mockSolutionIndicies) {
        mockReceiveData.push_back(std::make_pair(i, DATA[i]));
    }

    SelectiveData selectiveData(mockReceiveData);
    std::unique_ptr<MutableSubset> mockSolution(NaiveMutableSubset::makeNew());

    for (size_t i = 0; i < mockReceiveData.size(); i++) {
        mockSolution->addRow(i, 0);
    }

    auto translated = selectiveData.translateSolution(MutableSubset::upcast(move(mockSolution)));

    for (size_t i = 0; i < mockReceiveData.size(); i++) {
        CHECK(mockReceiveData[i].first == translated->getRow(i));
    }

    CHECK(mockReceiveData.size() == translated->size());
}

TEST_CASE("Testing adjacency list loader without values") {
    std::string listData = "0, 2\n0, 3\n2, 1\n2, 3\n3, 3";
    std::istringstream stream(listData);
    AsciiAdjacencyListDataLoader loader(stream, 4);
    auto data = loadData(loader);
    DEBUG_printData(data);
}