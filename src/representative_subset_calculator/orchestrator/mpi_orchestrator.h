#include "orchestrator.h"
#include <algorithm>
#include <mpi.h>

class MpiOrchestrator : public Orchestrator {

    public:
    static nlohmann::json buildDatasetOutputFromMachines(
        const BaseData &data,
        const AppData &appData
    ) {
        nlohmann::json outputData(Orchestrator::buildDatasetJson(data, appData));
        return aggregateJsonAtZero(outputData, appData.worldRank, appData.worldSize);
    }

    static nlohmann::json getTimersFromMachines(
        const Timers &localTimers,
        const int worldRank,
        const int worldSize
    ) {
        nlohmann::json localTimerJson = localTimers.outputToJson();
        return aggregateJsonAtZero(localTimerJson, worldRank, worldSize);
    }

    private:
    static nlohmann::json aggregateJsonAtZero(
        const nlohmann::json json,
        const int worldRank,
        const int worldSize
    ) {
        std::ostringstream outputStream;
        outputStream << json.dump();
        std::string localTimeData(outputStream.str());

        // send timer string lengths
        int localSendSize = localTimeData.size();
        std::vector<int> receiveSizes(worldSize, 0);
        MPI_Gather(&localSendSize, 1, MPI_INT, receiveSizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

        int receiveTotal = 0;
        std::vector<int> displacements(worldSize, 0);
        for (size_t rank = 0; rank < worldSize; rank++) {
            displacements[rank] = receiveTotal;
            receiveTotal += receiveSizes[rank];
        }

        std::vector<char> receiveBuffer(receiveTotal);

        // send timer strings
        MPI_Gatherv(
            localTimeData.data(), 
            localSendSize, 
            MPI_CHAR, 
            receiveBuffer.data(), 
            receiveSizes.data(), 
            displacements.data(), 
            MPI_CHAR, 
            0,
            MPI_COMM_WORLD
        );

        nlohmann::json output;
        if (worldRank == 0) {
            std::string timerData(receiveBuffer.begin(), receiveBuffer.end());

            for (size_t rank = 0; rank < worldSize; rank++) {
                const size_t end = rank == worldSize - 1 ? timerData.size() : displacements[rank + 1];
                std::string rawJson(timerData.substr(displacements[rank], end - displacements[rank]));
                output.push_back(nlohmann::json::parse(rawJson));
            }
        }

        // return as json
        return output;
    }

    public:
    static nlohmann::json buildMpiOutput(
        const AppData &appData, 
        const std::vector<std::unique_ptr<Subset>> &solution,
        const BaseData &data,
        const Timers &timers,
        const std::vector<unsigned int> &rowToRank
    ) {
        nlohmann::json output = Orchestrator::buildOutputBase(appData, solution, data, timers);
        output.push_back({"timers", getTimersFromMachines(timers, appData.worldRank, appData.worldSize)});
        output.push_back({"dataset", buildDatasetOutputFromMachines(data, appData)});

        return output;
    }

    static std::unique_ptr<BucketTitratorFactory> buildTitratorFactory(
        const AppData &appData, 
        const unsigned int threads,
        const RelevanceCalculatorFactory& calcFactory
    ) {
        if (appData.distributedAlgorithm == 1) {
            return std::unique_ptr<BucketTitratorFactory>(
                new SieveStreamingBucketTitratorFactory(threads, appData.distributedEpsilon, appData.outputSetSize, calcFactory)
            );
        }
        else if (appData.distributedAlgorithm == 2) {
            return std::unique_ptr<BucketTitratorFactory>(
                new ThreeSeiveBucketTitratorFactory(appData.distributedEpsilon, appData.threeSieveT, appData.outputSetSize, calcFactory)
            );
        } else {
            throw std::invalid_argument("ERROR: bad input");
        }
    }

    static std::unique_ptr<CandidateConsumer> buildConsumer(
        const AppData &appData, 
        const unsigned int threads, 
        const unsigned int numSenders,
        const RelevanceCalculatorFactory& calcFactory
    ) {
        std::unique_ptr<BucketTitratorFactory> titratorFactory(buildTitratorFactory(appData, threads, calcFactory));
        std::unique_ptr<BucketTitrator> titrator;
        
        // If we know that we're going to be sending all seeds, we do not know that the first m seeds from
        //  our m senders will contain deltaZero. We must build with dynamic buckets in this case.
        // 
        // Also, we should set the number of senders for the consumer to be 0. Otherwise the consumer will expect to 
        //  wait for seeds before starting inserts, which in this case is a de-optimization.
        if (appData.sendAllToReceiver) {
            titrator = std::unique_ptr<BucketTitrator>(titratorFactory->createWithDynamicBuckets());
        } else {
            titrator = std::unique_ptr<BucketTitrator>(new LazyInitializingBucketTitrator(move(titratorFactory), calcFactory));
        }
        return std::unique_ptr<NaiveCandidateConsumer>(new NaiveCandidateConsumer(move(titrator), appData.sendAllToReceiver ? 0 : numSenders));
    }
};