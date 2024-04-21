#include <vector>
#include <cstdlib>
#include "orchestrator.h"

void validateValidRanks(const std::vector<unsigned int> &allRanks, const unsigned int worldSize) {
    for (const auto & rank : allRanks) {
        CHECK(rank >= 0);
        CHECK(rank < worldSize);
    }
}

TEST_CASE("Testing the proper blocking of a dataset with an evenly divisible dataset") {
    const unsigned int WORLD_SIZE = DENSE_DATA.size();

    AppData appData;
    appData.numberOfDataRows = DENSE_DATA.size();
    appData.worldSize = WORLD_SIZE;
    
    std::vector<unsigned int> previous;
    std::srand((unsigned) time(NULL));
    int seed = std::rand();

    for (unsigned int rank = 0; rank < WORLD_SIZE; rank++) {
        appData.worldRank = rank;
        auto allRanks = Orchestrator::getRowToRank(appData, seed);
        if (previous.size() == 0) {
            previous = allRanks;
        }
        CHECK(previous == allRanks);
        CHECK(allRanks.size() == appData.numberOfDataRows);
        validateValidRanks(allRanks, appData.worldSize);
    }
}
