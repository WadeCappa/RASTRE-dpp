#include <doctest/doctest.h>
#include "bucket.h"

static const double EVERYTHING_ALLOWED_THRESHOLD = 0.0000001;
static const double EVERYTHING_DISALLOWED_THRESHOLD = 0.999;
static const int K = 3;

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