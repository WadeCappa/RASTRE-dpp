#include "data_loader.h"
#include <doctest/doctest.h>

static std::vector<std::vector<double>> DATA = {
    {4,17,20},
    {5,7,31},
    {2.632125,5.12566,763}
};

static std::string dataToString(const std::vector<std::vector<double>> &data) {
    std::string output = "";
    for (const auto & row : data) {
        for (const auto & v : row) {
            output += std::to_string(v) + ",";
        }
        output.pop_back();
        output.append("\n");
    }

    return output;
}

TEST_CASE("Data tools tests") {
    std::istringstream inputStream(dataToString(DATA));
    auto result = DataLoader::loadData(inputStream);
    CHECK(result == DATA);
}