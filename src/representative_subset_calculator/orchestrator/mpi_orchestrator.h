#include "orchestrator.h"
#include <algorithm>
#include <mpi.h>

class MpiOrchestrator : public Orchestrator {

    public:
    static nlohmann::json getTimersFromMachines(
        const Timers &localTimers,
        const int worldRank,
        const int worldSize
    ) {
        nlohmann::json localTimerJson = localTimers.outputToJson();
        std::ostringstream outputStream;
        outputStream << localTimerJson.dump();
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

    static nlohmann::json buildMpiOutput(
        const AppData &appData, 
        const Subset &solution,
        const BaseData &data,
        const Timers &timers,
        const std::vector<unsigned int> &rowToRank
    ) {
        nlohmann::json output = Orchestrator::buildOutputBase(appData, solution, data, timers);

        output.push_back({"timers", getTimersFromMachines(timers, appData.worldRank, appData.worldSize)});

        return output;
    }

    static std::unique_ptr<CandidateConsumer> buildConsumer(const AppData &appData, const unsigned int threads, const unsigned int numSenders) {
        BucketTitrator* titrator;
        if (appData.distributedAlgorithm == 1) {
            titrator = new SieveStreamingBucketTitrator(threads, appData.distributedEpsilon, appData.outputSetSize);
        }
        else if (appData.distributedAlgorithm == 2) {
            titrator = new ThreeSieveBucketTitrator(appData.distributedEpsilon, appData.threeSieveT, appData.outputSetSize);
        }

        return std::unique_ptr<NaiveCandidateConsumer>(
            new NaiveCandidateConsumer(
                std::unique_ptr<BucketTitrator>(titrator),
                numSenders,
                std::unique_ptr<RelevanceCalculatorFactory>(new NaiveRelevanceCalculatorFactory()))
            );
    }
};