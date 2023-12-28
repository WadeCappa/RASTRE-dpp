#include "data_loader.h"
#include <doctest/doctest.h>

static std::vector<std::vector<double>> DATA = {
    {4,17,20},
    {5,7,31},
    {2.632125,5.12566,763}
};

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