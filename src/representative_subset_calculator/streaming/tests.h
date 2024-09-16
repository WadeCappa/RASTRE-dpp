#include <doctest/doctest.h>

#include "synchronous_queue.h"
#include "bucket.h"
#include "candidate_seed.h"
#include "bucket_titrator.h"
#include "candidate_consumer.h"
#include "rank_buffer.h"
#include "receiver_interface.h"
#include "greedy_streamer.h"

static const float EVERYTHING_ALLOWED_THRESHOLD = 0.0000001;
static const float EVERYTHING_DISALLOWED_THRESHOLD = 0.999;
static const int K = 3;
static const int T = 1;
static float EPSILON = 0.5;

std::vector<std::unique_ptr<BucketTitrator>> getTitrators() {
    std::vector<std::unique_ptr<BucketTitrator>> res;
    res.push_back(SieveStreamingBucketTitrator::createWithDynamicBuckets(1, EPSILON, DENSE_DATA.size()));
    res.push_back(ThreeSieveBucketTitrator::createWithDynamicBuckets(EPSILON, T, DENSE_DATA.size()));
    return move(res);
}

std::vector<std::unique_ptr<BucketTitratorFactory>> getTitratorFactories() {
    std::vector<std::unique_ptr<BucketTitratorFactory>> res;
    res.push_back(std::unique_ptr<BucketTitratorFactory>(new SieveStreamingBucketTitratorFactory(1, EPSILON, DENSE_DATA.size())));
    res.push_back(std::unique_ptr<BucketTitratorFactory>(new ThreeSeiveBucketTitratorFactory(EPSILON, T, DENSE_DATA.size())));
    return move(res);
}

std::unique_ptr<CandidateSeed> buildSeed(const size_t row, const unsigned int rank) {
    return std::unique_ptr<CandidateSeed>(
        new CandidateSeed(
            row, 
            DenseDataRow::of(DENSE_DATA[row]), 
            rank
        )
    );
}

std::unique_ptr<CandidateSeed> buildSeed() {
    const size_t row = 0;
    const unsigned int rank = 0;

    return buildSeed(row, rank);
}

std::unique_ptr<BucketTitrator> getTitrator(unsigned int numThreads, float eps, unsigned int k) {
    return SieveStreamingBucketTitrator::createWithDynamicBuckets(numThreads, eps, k);
}

std::unique_ptr<NaiveCandidateConsumer> getConsumer(std::unique_ptr<BucketTitrator> titrator, size_t worldSize) {
    return NaiveCandidateConsumer::from(move(titrator), worldSize);
}

class FakeReceiver : public Receiver {
    private:
    size_t nextIndex;
    unsigned int nextRank;
    const unsigned int worldSize;

    public:
    FakeReceiver(const unsigned int worldSize) : nextIndex(0), worldSize(worldSize), nextRank(0) {}

    std::unique_ptr<CandidateSeed> receiveNextSeed(std::atomic_bool &stillReceiving) {
        std::unique_ptr<CandidateSeed> nextSeed(buildSeed(nextIndex, nextRank));
        nextIndex++;
        nextRank = (nextRank + 1) % worldSize;

        stillReceiving.store(nextIndex < DENSE_DATA.size());

        return nextSeed;
    }

    std::unique_ptr<Subset> getBestReceivedSolution() {
        return NaiveMutableSubset::makeNew();
    }
};

class FakeRankBuffer : public RankBuffer {
    private:
    const unsigned int rank;
    const unsigned int worldSize;
    const size_t totalGlobalRows;
    const unsigned int totalSeedsToSend;

    size_t nextRowToReturn;
    bool shouldReturnSeed;
    unsigned int seedsSent;
    std::unique_ptr<MutableSubset> rankSolution;

    public:
    FakeRankBuffer(
        const unsigned int rank, 
        const unsigned int worldSize
    ) : 
        shouldReturnSeed(false), 
        nextRowToReturn(0), 
        rank(rank), 
        worldSize(worldSize), 
        totalGlobalRows(DENSE_DATA.size()),
        totalSeedsToSend(DENSE_DATA.size() / worldSize),
        seedsSent(0),
        rankSolution(std::unique_ptr<MutableSubset>(NaiveMutableSubset::makeNew()))
    {}

    CandidateSeed* askForData() {
        if (this->shouldReturnSeed) {
            if ((this->nextRowToReturn + this->rank) % this->worldSize == 0) {
                size_t row = this->nextRowToReturn;
                CandidateSeed *nextSeed = new CandidateSeed(row, DenseDataRow::of(DENSE_DATA[row]), this->rank);
                this->shouldReturnSeed = false;
                this->nextRowToReturn++;
                this->seedsSent++;
                this->rankSolution->addRow(row, 1);
                return nextSeed;
            } else {
                this->nextRowToReturn++;
            }
        } else {
            this->shouldReturnSeed = true;
        }
        return nullptr;
    }

    bool stillReceiving() {
        return this->seedsSent < this->totalSeedsToSend;
    }

    unsigned int getRank() const {
        return this->rank;
    }

    float getLocalSolutionScore() const {
        return this->rankSolution->getScore();
    }

    std::unique_ptr<Subset> getLocalSolutionDestroyBuffer() {
        return move(this->rankSolution);
    }
};

void assertSolutionIsValid(std::unique_ptr<Subset> solution, const size_t dataSize) {
    CHECK(solution->getScore() > 0);
    CHECK(solution->size() > 0);
    CHECK(solution->size() <= dataSize);

    std::set<size_t> seen;
    for (const auto & seed : *solution.get()) {
        const auto & row = seed;
        CHECK(seen.find(row) == seen.end());
        seen.insert(row);
        CHECK(row >= 0);
        CHECK(row < dataSize);
    }
}

void assertSolutionsEqual(std::unique_ptr<Subset> a, std::unique_ptr<Subset> b, const size_t dataSize) {
    CHECK(a->size() == b->size());
    CHECK(a->getScore() == b->getScore());

    assertSolutionIsValid(move(a), dataSize);
    assertSolutionIsValid(move(b), dataSize);
}

std::vector<std::unique_ptr<RankBuffer>> buildFakeBuffers(const unsigned int worldSize) {
    std::vector<std::unique_ptr<RankBuffer>> buffers;
    for (size_t rank = 0; rank < worldSize; rank++) {
        buffers.push_back(std::unique_ptr<RankBuffer>(new FakeRankBuffer(rank, worldSize)));
    }

    return buffers;
}

void evaluateTitrator(std::unique_ptr<BucketTitrator> titrator) {
    Timers timers;
    
    // Our fake receivers use this data. We can assume this is true here.
    const size_t dataSize = DENSE_DATA.size();
    const unsigned int worldSize = dataSize / 2;

    NaiveReceiver receiver(buildFakeBuffers(worldSize));
    std::unique_ptr<NaiveCandidateConsumer> consumer(getConsumer(move(titrator), worldSize));
    std::unique_ptr<Subset> solution((SeiveGreedyStreamer(receiver, *consumer, timers, false)).resolveStream());
    assertSolutionIsValid(move(solution), dataSize);
}

TEST_CASE("Bucket can insert at threshold") {
    ThresholdBucket bucket(EVERYTHING_ALLOWED_THRESHOLD, K);
    std::unique_ptr<DataRow> data(DenseDataRow::of(DENSE_DATA[0]));
    CHECK(bucket.attemptInsert(0, *data));
    CHECK(bucket.getUtility() > 0);
}

TEST_CASE("Bucket cannot insert when k == 0") {
    ThresholdBucket bucketWithKZero(EVERYTHING_ALLOWED_THRESHOLD, 0);
    std::unique_ptr<DataRow> data(DenseDataRow::of(DENSE_DATA[0]));
    CHECK(bucketWithKZero.attemptInsert(0, *data) == false);
    CHECK(bucketWithKZero.getUtility() == 0);
}

TEST_CASE("Bucket cannot insert when threshold is large") {
    ThresholdBucket bucketWithHighThreshold(EVERYTHING_ALLOWED_THRESHOLD, 0);
    std::unique_ptr<DataRow> data(DenseDataRow::of(DENSE_DATA[0]));
    CHECK(bucketWithHighThreshold.attemptInsert(0, *data) == false);
    CHECK(bucketWithHighThreshold.getUtility() == 0);
}

TEST_CASE("Bucket returns valid solution") {
    ThresholdBucket bucket(EVERYTHING_ALLOWED_THRESHOLD, K);
    std::unique_ptr<DataRow> data(DenseDataRow::of(DENSE_DATA[0]));
    CHECK(bucket.attemptInsert(0, *data));
    std::unique_ptr<Subset> solution(bucket.returnSolutionDestroyBucket());
    CHECK(solution->size() == 1);
    CHECK(solution->getRow(0) == 0);
    CHECK(solution->getScore() > 0);
}

TEST_CASE("Candidate seed can exist") {
    const size_t row = 0;
    const auto & dataRow = DENSE_DATA[row];
    const unsigned int rank = 0;

    std::unique_ptr<CandidateSeed> seed(buildSeed(row, rank));
    
    CHECK(seed->getRow() == row);
    CHECK(seed->getOriginRank() == rank);
}

TEST_CASE("Consumer can process seeds") {
    const unsigned int worldSize = 1;
    std::unique_ptr<NaiveCandidateConsumer> consumer(getConsumer(getTitrator(5, EPSILON, 1), worldSize));
    Timers timers;
    SynchronousQueue<std::unique_ptr<CandidateSeed>> queue;
    std::unique_ptr<CandidateSeed> seed(buildSeed());
    const unsigned int globalRow = seed->getRow();

    queue.push(move(seed));
    consumer->accept(queue, timers);

    std::unique_ptr<Subset> solution(consumer->getBestSolutionDestroyConsumer());
    
    CHECK(solution->getScore() > 0);
    CHECK(solution->size() == 1);
    CHECK(solution->getRow(0) == globalRow);
}

TEST_CASE("Consumer can find a solution") {
    const unsigned int worldSize = DENSE_DATA.size();
    std::unique_ptr<NaiveCandidateConsumer> consumer(getConsumer(getTitrator(5, EPSILON, worldSize), worldSize));
    SynchronousQueue<std::unique_ptr<CandidateSeed>> queue;
    Timers timers;
    for (size_t i = 0; i < worldSize; i++) {
        queue.push(buildSeed(i,i));
        consumer->accept(queue, timers);
    }

    std::unique_ptr<Subset> solution(consumer->getBestSolutionDestroyConsumer());
    assertSolutionIsValid(move(solution), DENSE_DATA.size());
}

TEST_CASE("Test fake receiver") {
    const unsigned int worldSize = DENSE_DATA.size() / 2;
    FakeReceiver receiver(worldSize);

    size_t count = 0;
    std::atomic_bool stillReceiving = true; 
    while (stillReceiving) {
        count++;
        std::unique_ptr<CandidateSeed> nextSeed(receiver.receiveNextSeed(stillReceiving));
        CHECK(nextSeed->getRow() >= 0);
        CHECK(nextSeed->getRow() < DENSE_DATA.size());
        CHECK(nextSeed->getOriginRank() >= 0);
        CHECK(nextSeed->getOriginRank() < worldSize);
    }

    CHECK(count == DENSE_DATA.size());
}

TEST_CASE("Testing streaming with fake receiver") {
    const unsigned int worldSize = DENSE_DATA.size() / 2;
    FakeReceiver receiver(worldSize);
    std::unique_ptr<NaiveCandidateConsumer> consumer(getConsumer(getTitrator(5, EPSILON, DENSE_DATA.size()), worldSize));

    Timers timers;
    SeiveGreedyStreamer streamer(receiver, *consumer, timers, false);
    std::unique_ptr<Subset> solution(streamer.resolveStream());
    assertSolutionIsValid(move(solution), DENSE_DATA.size());
}

TEST_CASE("Testing the fake buffer") {
    std::unique_ptr<RankBuffer> fakeBuffer(new FakeRankBuffer(0, 1));

    size_t expectedRow = 0;
    while (fakeBuffer->stillReceiving()) {
        CandidateSeed* nextElement = fakeBuffer->askForData();
        if (nextElement != nullptr) {
            CHECK(nextElement->getRow() == expectedRow);
            expectedRow++;
        }
    }

    CHECK(expectedRow == DENSE_DATA.size());
}

TEST_CASE("Testing the naiveReceiver with fake buffers") {
    const unsigned int worldSize = 1;
    NaiveReceiver receiver(buildFakeBuffers(1));

    size_t expectedRow = 0;
    std::atomic_bool moreData = true;
    while (moreData) {
        std::unique_ptr<CandidateSeed> nextElement = receiver.receiveNextSeed(moreData);
        CHECK(nextElement->getRow() == expectedRow);
        expectedRow++;
    }

    CHECK(expectedRow == DENSE_DATA.size());
}

TEST_CASE("Testing multiple buffers") {
    const unsigned int worldSize = DENSE_DATA.size()/2;
    NaiveReceiver receiver(buildFakeBuffers(worldSize));

    size_t expectedRow = 0;
    std::atomic_bool moreData = true;
    while (moreData) {
        std::unique_ptr<CandidateSeed> nextElement = receiver.receiveNextSeed(moreData);
        CHECK(nextElement->getRow() == expectedRow);
        expectedRow++;
    }

    CHECK(expectedRow == DENSE_DATA.size());
}

TEST_CASE("Testing end to end without MPI") {
    LoggerHelper::setupLoggers();
    std::vector<std::unique_ptr<BucketTitratorFactory>> titrators(getTitratorFactories());
    for (size_t i = 0; i < titrators.size(); i++) {
        evaluateTitrator(move(titrators[i]->createWithDynamicBuckets()));
    }

}

TEST_CASE("Streaming with decorator") {
    std::vector<std::unique_ptr<BucketTitratorFactory>> titrators(getTitratorFactories());
    for (size_t i = 0; i < titrators.size(); i++) {
        std::unique_ptr<BucketTitrator> decorator(new LazyInitializingBucketTitrator (move(titrators[i])));
        evaluateTitrator(move(decorator));
    }
}

TEST_CASE("Comparing titrators") {
    // LoggerHelper::setupLoggers();
    // const unsigned int worldSize = DENSE_DATA.size()/2;
    // NaiveReceiver receiver_a(buildFakeBuffers(worldSize));
    // NaiveReceiver receiver_b(buildFakeBuffers(worldSize));

    // std::unique_ptr<NaiveCandidateConsumer> threeSeiveConsumer(getConsumer(ThreeSieveBucketTitrator::createWithDynamicBuckets(EPSILON, T, DENSE_DATA.size()), worldSize));
    // std::unique_ptr<NaiveCandidateConsumer> seiveStreamingConsumer(getConsumer(SieveStreamingBucketTitrator::createWithDynamicBuckets(1, EPSILON, DENSE_DATA.size()), worldSize));

    // std::unique_ptr<Subset> threeSeiveSolution((SeiveGreedyStreamer(receiver_a, *threeSeiveConsumer, timers, false)).resolveStream());
    // std::unique_ptr<Subset> seiveStreamingSolution((SeiveGreedyStreamer(receiver_b, *seiveStreamingConsumer, timers, false)).resolveStream());
    // assertSolutionsEqual(move(threeSeiveSolution), move(seiveStreamingSolution), DENSE_DATA.size());
}