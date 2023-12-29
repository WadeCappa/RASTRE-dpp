#include "data_loader.h"
#include "normalizer.h"

#include <utility>
#include <functional>
#include <doctest/doctest.h>

static std::vector<std::vector<double>> DATA = {
    {4,17,20},
    {5,7,31},
    {2.632125,5.12566,763}
};

static std::pair<std::vector<double>, double> VECTOR_WITH_LENGTH = std::make_pair(
    std::vector<double>{3.0, 3.0, 3.0, 3.0},
    6.0
);

static std::string rowToString(const std::vector<double> &row) {
    std::string output = "";
    for (const auto & v : row) {
        output += std::to_string(v) + ",";
    }
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

TEST_CASE("Testing buildElement") {
    for (const auto & e : DATA) {
        std::string row = rowToString(e);
        CHECK(DataLoader::buildElement(row) == e);
    }
}

TEST_CASE("Testing loading data") {
    std::istringstream inputStream(matrixToString(DATA));

    std::vector<std::vector<double>> data;
    AsciiDataLoader loader(inputStream);

    while (loader.loadNext()) {
        data.push_back(loader.returnLoaded());
    }

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

        for (const auto & e : copy) {
            CHECK(e <= 1.0);
        }
    }
}