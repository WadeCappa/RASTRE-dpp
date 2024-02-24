#include <doctest/doctest.h>

#include "bucket.h"
#include "candidate_seed.h"
#include "candidate_consumer.h"

static const double EVERYTHING_ALLOWED_THRESHOLD = 0.0000001;
static const double EVERYTHING_DISALLOWED_THRESHOLD = 0.999;
static const int K = 3;
static double EPSILON = 0.5;

std::unique_ptr<CandidateSeed> buildSeed(const size_t row, const unsigned int rank) {
    return std::unique_ptr<CandidateSeed>(new CandidateSeed(row, DATA[row], rank));
}

std::unique_ptr<CandidateSeed> buildSeed() {
    const size_t row = 0;
    const unsigned int rank = 0;

    return buildSeed(row, rank);
}

TEST_CASE("Bucket can insert at threshold") {
    ThresholdBucket bucket(EVERYTHING_ALLOWED_THRESHOLD, K);
    CHECK(bucket.attemptInsert(0, DATA[0]));
    CHECK(bucket.getUtility() > 0);
}

TEST_CASE("Bucket cannot insert when k == 0") {
    ThresholdBucket bucketWithKZero(EVERYTHING_ALLOWED_THRESHOLD, 0);
    CHECK(bucketWithKZero.attemptInsert(0, DATA[0]) == false);
    CHECK(bucketWithKZero.getUtility() == 0);
}

TEST_CASE("Bucket cannot insert when threshold is large") {
    ThresholdBucket bucketWithHighThreshold(EVERYTHING_ALLOWED_THRESHOLD, 0);
    CHECK(bucketWithHighThreshold.attemptInsert(0, DATA[0]) == false);
    CHECK(bucketWithHighThreshold.getUtility() == 0);
}

TEST_CASE("Bucket returns valid solution") {
    ThresholdBucket bucket(EVERYTHING_ALLOWED_THRESHOLD, K);
    CHECK(bucket.attemptInsert(0, DATA[0]));
    std::unique_ptr<Subset> solution(bucket.returnSolutionDestroyBucket());
    CHECK(solution->size() == 1);
    CHECK(solution->getRow(0) == 0);
    CHECK(solution->getScore() > 0);
}

TEST_CASE("Candidate seed can exist") {
    const size_t row = 0;
    const auto & dataRow = DATA[row];
    const unsigned int rank = 0;

    std::unique_ptr<CandidateSeed> seed(buildSeed(row, rank));
    
    CHECK(seed->getData() == dataRow);
    CHECK(seed->getRow() == row);
    CHECK(seed->getOriginRank() == rank);
}

TEST_CASE("Consumer can process seeds") {
    const unsigned int worldSize = 1;
    SeiveCandidateConsumer consumer(worldSize, 1, EPSILON);

    std::unique_ptr<CandidateSeed> seed(buildSeed());
    const unsigned int globalRow = seed->getRow();
    consumer.accept(move(seed));
    std::unique_ptr<Subset> solution(consumer.getBestSolutionDestroyConsumer());
    
    CHECK(solution->getScore() > 0);
    CHECK(solution->size() == 1);
    CHECK(solution->getRow(0) == globalRow);
}

TEST_CASE("Consumer can find a solution") {
    const unsigned int worldSize = DATA.size();
    SeiveCandidateConsumer consumer(worldSize, worldSize, EPSILON);

    for (size_t i = 0; i < worldSize; i++) {
        consumer.accept(buildSeed(i, i));
    }

    std::unique_ptr<Subset> solution(consumer.getBestSolutionDestroyConsumer());
    CHECK(solution->getScore() > 0);
    CHECK(solution->size() > 0);

    std::set<size_t> seen;
    for (const auto & seed : *solution.get()) {
        const auto & row = seed;
        CHECK(seen.find(row) == seen.end());
        seen.insert(row);
    }

    std::cout << solution->toJson().dump(2) << std::endl;
}