#include "orchestrator.h"
#include <mpi.h>

class MpiOrchestrator : public Orchestrator {

    public:

    class Metadata {

        private:
        double rowsPerRank;
        // double nnzsPerRank;
        double barrierTime;
        double totalCalculationTime;
        double localCalculationTime;
        double globalCalculationTime;
        double communicationTime;
        double bufferEncodingTime;
        double bufferDecodingTime;
        double loadingDatasetTime;

        public:

        Metadata(double rowsPerRank, 
                // double nnzsPerRank, 
                double barrierTime, 
                double totalCalculationTime, 
                double localCalculationTime, 
                double globalCalculationTime, 
                double communicationTime, 
                double bufferEncodingTime, 
                double bufferDecodingTime, 
                double loadingDatasetTime) : 
                
                rowsPerRank(rowsPerRank), 
                // nnzsPerRank(nnzsPerRank), 
                barrierTime(barrierTime), 
                totalCalculationTime(totalCalculationTime), 
                localCalculationTime(localCalculationTime), 
                globalCalculationTime(globalCalculationTime), 
                communicationTime(communicationTime), 
                bufferEncodingTime(bufferEncodingTime), 
                bufferDecodingTime(bufferDecodingTime), 
                loadingDatasetTime(loadingDatasetTime)   {}


        std::vector<double> sendMetadata(int worldSize, MPI_Comm comm) {

            std::vector<double> sendBufferMetadata;
            
            sendBufferMetadata.push_back(rowsPerRank);
            // sendBufferMetadata.push_back(nnzsPerRank);
            sendBufferMetadata.push_back(barrierTime);
            sendBufferMetadata.push_back(totalCalculationTime);
            sendBufferMetadata.push_back(localCalculationTime);
            sendBufferMetadata.push_back(globalCalculationTime);
            sendBufferMetadata.push_back(communicationTime);
            sendBufferMetadata.push_back(bufferEncodingTime);
            sendBufferMetadata.push_back(loadingDatasetTime);

            std::vector<double> receiveBufferMetadata(worldSize * sendBufferMetadata.size());
            std::vector<int> count(worldSize, sendBufferMetadata.size());
            std::vector<int> displacements(worldSize);

            for(size_t i = 0; i < count.size(); i++)
                displacements[i] = i * count[i];

            MPI_Gatherv(
                sendBufferMetadata.data(), 
                sendBufferMetadata.size(), 
                MPI_DOUBLE, 
                receiveBufferMetadata.data(), 
                count.data(), 
                displacements.data(),
                MPI_DOUBLE, 
                0, 
                comm
            );

            return receiveBufferMetadata;

        }


        nlohmann::json buildMpiOutputWithMetadata(
            const AppData &appData, 
            const RepresentativeSubset &solution,
            const Data &data,
            const Timers &timers, 
            int worldSize, 
            MPI_Comm comm
        ) {
            nlohmann::json output = Orchestrator::buildOutputBase(appData, solution, data, timers);
            std::vector<double> receiveBufferMetadata = sendMetadata(worldSize, comm);


            // Logic to interpret receiveBufferMetadata and insert into JSON goes here. 



            return output;
        }
    };
 
    
    static nlohmann::json buildMpiOutput(
        const AppData &appData, 
        const RepresentativeSubset &solution,
        const Data &data,
        const Timers &timers
    ) {
        nlohmann::json output = Orchestrator::buildOutputBase(appData, solution, data, timers);
        return output;
    }
};