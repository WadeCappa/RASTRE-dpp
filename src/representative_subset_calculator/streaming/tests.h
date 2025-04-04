#include <doctest/doctest.h>

#include "synchronous_queue.h"
#include "bucket.h"
#include "candidate_seed.h"
#include "bucket_titrator.h"
#include "candidate_consumer.h"
#include "rank_buffer.h"
#include "receiver_interface.h"
#include "naive_receiver.h"
#include "greedy_streamer.h"

static const float EVERYTHING_ALLOWED_THRESHOLD = 0.0000001;
static const float EVERYTHING_DISALLOWED_THRESHOLD = 0.999;
static const int K = 3;
static const int T = 1;
static float EPSILON = 0.5;

std::vector<std::unique_ptr<BucketTitrator>> getTitrators(const RelevanceCalculatorFactory& calcFactory) {
    std::vector<std::unique_ptr<BucketTitrator>> res;
    res.push_back(SieveStreamingBucketTitrator::createWithDynamicBuckets(1, EPSILON, DENSE_DATA.size(), calcFactory));
    res.push_back(ThreeSieveBucketTitrator::createWithDynamicBuckets(EPSILON, T, DENSE_DATA.size(), calcFactory));
    return std::move(res);
}

std::vector<std::unique_ptr<BucketTitratorFactory>> getTitratorFactories(const RelevanceCalculatorFactory& calcFactory) {
    std::vector<std::unique_ptr<BucketTitratorFactory>> res;
    res.push_back(std::unique_ptr<BucketTitratorFactory>(new SieveStreamingBucketTitratorFactory(1, EPSILON, DENSE_DATA.size(), calcFactory)));
    res.push_back(std::unique_ptr<BucketTitratorFactory>(new ThreeSeiveBucketTitratorFactory(EPSILON, T, DENSE_DATA.size(), calcFactory)));
    return std::move(res);
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

std::unique_ptr<BucketTitrator> getTitrator(
    unsigned int numThreads, float eps, unsigned int k, const RelevanceCalculatorFactory& calcFactory
) {
    return SieveStreamingBucketTitrator::createWithDynamicBuckets(numThreads, eps, k, calcFactory);
}

std::unique_ptr<NaiveCandidateConsumer> getConsumer(std::unique_ptr<BucketTitrator> titrator, size_t worldSize) {
    return NaiveCandidateConsumer::from(std::move(titrator), worldSize);
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
    
    // How many times nullptr is returned for every real seed
    const unsigned int offset;

    unsigned int seedsSent;
    
    // Represents how many times this object has been asked for 
    // data. Used with the offset to determine if data should
    // be returned.
    unsigned int wait;
    std::unique_ptr<Subset> seeds;

    public:
    FakeRankBuffer(
        const unsigned int rank, 
        const unsigned int offset,
        std::unique_ptr<Subset> seeds
    ) : 
        rank(rank), 
        offset(offset),
        seeds(std::move(seeds)),
        seedsSent(0),
        wait(0)
    {}

    CandidateSeed* askForData() {
        if (this->seedsSent >= this->seeds->size()) {
            this->seedsSent++;
            return nullptr;
        }

        if (this->wait > 0) {
            this->wait--;
            return nullptr;
        }

        size_t row = this->seeds->getRow(seedsSent);
        CandidateSeed *nextSeed = new CandidateSeed(row, DenseDataRow::of(DENSE_DATA[row]), this->rank);
        this->seedsSent++;
        this->wait = this->offset;
        return nextSeed;
    }

    bool stillReceiving() {
        return this->seedsSent <= this->seeds->size();
    }

    unsigned int getRank() const {
        return this->rank;
    }

    float getLocalSolutionScore() const {
        return this->seeds->getScore();
    }

    std::unique_ptr<Subset> getLocalSolutionDestroyBuffer() {
        return std::move(this->seeds);
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
    CHECK(a->getScore() > b->getScore() - LARGEST_ACCEPTABLE_ERROR);
    CHECK(a->getScore() < b->getScore() + LARGEST_ACCEPTABLE_ERROR);
    assertSolutionIsValid(std::move(a), dataSize);
    assertSolutionIsValid(std::move(b), dataSize);
}

std::vector<std::unique_ptr<RankBuffer>> buildFakeBuffers(const unsigned int worldSize) {
    std::vector<std::unique_ptr<RankBuffer>> buffers;
    for (size_t rank = 0; rank < worldSize; rank++) {
        buffers.push_back(std::unique_ptr<RankBuffer>(
            new FakeRankBuffer(rank, worldSize, Subset::ofCopy(std::vector<size_t>{rank}, 5))
        ));
    }

    return buffers;
}

std::unique_ptr<Subset> getSolution(std::unique_ptr<BucketTitrator> titrator, const size_t worldSize) {
    Timers timers;
    
    NaiveReceiver receiver(buildFakeBuffers(worldSize));
    std::unique_ptr<NaiveCandidateConsumer> consumer(getConsumer(std::move(titrator), worldSize));
    std::unique_ptr<Subset> solution((SeiveGreedyStreamer(receiver, *consumer, timers, false)).resolveStream());
    return std::move(solution);
}

void evaluateTitrator(std::unique_ptr<BucketTitrator> titrator) {
    // Our fake receivers use this data. We can assume this is true here.
    const size_t dataSize = DENSE_DATA.size();
    const unsigned int worldSize = dataSize / 2;

    assertSolutionIsValid(getSolution(std::move(titrator), worldSize), dataSize);
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
    NaiveRelevanceCalculatorFactory calcFactory;
    std::unique_ptr<NaiveCandidateConsumer> consumer(getConsumer(getTitrator(5, EPSILON, 1, calcFactory), worldSize));
    Timers timers;
    SynchronousQueue<std::unique_ptr<CandidateSeed>> queue;
    std::unique_ptr<CandidateSeed> seed(buildSeed());
    const unsigned int globalRow = seed->getRow();

    queue.push(std::move(seed));
    consumer->accept(queue, timers);

    std::unique_ptr<Subset> solution(consumer->getBestSolutionDestroyConsumer());
    
    CHECK(solution->getScore() > 0);
    CHECK(solution->size() == 1);
    CHECK(solution->getRow(0) == globalRow);
}

TEST_CASE("Consumer can find a solution") {
    const unsigned int worldSize = DENSE_DATA.size();
    NaiveRelevanceCalculatorFactory calcFactory;
    std::unique_ptr<NaiveCandidateConsumer> consumer(getConsumer(getTitrator(5, EPSILON, worldSize, calcFactory), worldSize));
    SynchronousQueue<std::unique_ptr<CandidateSeed>> queue;
    Timers timers;
    for (size_t i = 0; i < worldSize; i++) {
        queue.push(buildSeed(i,i));
        consumer->accept(queue, timers);
    }

    std::unique_ptr<Subset> solution(consumer->getBestSolutionDestroyConsumer());
    assertSolutionIsValid(std::move(solution), DENSE_DATA.size());
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
    NaiveRelevanceCalculatorFactory calcFactory;
    std::unique_ptr<NaiveCandidateConsumer> consumer(getConsumer(getTitrator(5, EPSILON, DENSE_DATA.size(), calcFactory), worldSize));

    Timers timers;
    SeiveGreedyStreamer streamer(receiver, *consumer, timers, false);
    std::unique_ptr<Subset> solution(streamer.resolveStream());
    assertSolutionIsValid(std::move(solution), DENSE_DATA.size());
}

TEST_CASE("Testing the fake buffer") {
    std::vector<size_t> data{0, 1, 2, 3};
    std::unique_ptr<RankBuffer> fakeBuffer(new FakeRankBuffer(
        0, 0, Subset::of(data, 20))
    );

    size_t expectedRow = 0;
    while (fakeBuffer->stillReceiving()) {
        CandidateSeed* nextElement = fakeBuffer->askForData();
        if (nextElement != nullptr) {
            CHECK(nextElement->getRow() == expectedRow);
            expectedRow++;
        }
    }

    CHECK(expectedRow == data.size());
}

TEST_CASE("Testing the naiveReceiver with fake buffers") {
    const unsigned int worldSize = 1;
    NaiveReceiver receiver(buildFakeBuffers(worldSize));

    size_t expectedRow = 0;
    std::atomic_bool moreData = true;
    while (moreData) {
        std::unique_ptr<CandidateSeed> nextElement = receiver.receiveNextSeed(moreData);
        if (nextElement != nullptr) {
            CHECK(nextElement->getRow() == expectedRow);
            expectedRow++;
        }
    }

    CHECK(expectedRow == worldSize);
}

TEST_CASE("Testing end to end without MPI") {
    NaiveRelevanceCalculatorFactory calcFactory;
    std::vector<std::unique_ptr<BucketTitratorFactory>> titrators(getTitratorFactories(calcFactory));
    for (size_t i = 0; i < titrators.size(); i++) {
        evaluateTitrator(std::move(titrators[i]->createWithDynamicBuckets()));
    }
}

TEST_CASE("Streaming with decorator") {
    NaiveRelevanceCalculatorFactory calcFactory;
    std::vector<std::unique_ptr<BucketTitratorFactory>> titrators(getTitratorFactories(calcFactory));
    for (size_t i = 0; i < titrators.size(); i++) {
        std::unique_ptr<BucketTitrator> decorator(new LazyInitializingBucketTitrator(std::move(titrators[i]), calcFactory));
        evaluateTitrator(std::move(decorator));
    }
}

TEST_CASE("Comparing titrators") {
    NaiveRelevanceCalculatorFactory calcFactory;
    std::vector<std::unique_ptr<BucketTitratorFactory>> titrators(getTitratorFactories(calcFactory));
    const size_t worldSize = 1;
    std::vector<std::unique_ptr<Subset>> solutions;
    for (size_t i = 0; i < titrators.size(); i++) {
        std::unique_ptr<BucketTitrator> decorator(new LazyInitializingBucketTitrator(std::move(titrators[i]), calcFactory));
        solutions.push_back(getSolution(std::move(decorator), worldSize));
    }
    assertSolutionsEqual(std::move(solutions[0]), std::move(solutions[1]), DENSE_DATA.size());
}