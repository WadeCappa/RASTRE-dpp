#include "representative_subset_calculator/streaming/communication_constants.h"
#include "representative_subset_calculator/representative_subset.h"
#include "data_tools/data_row_visitor.h"
#include "data_tools/dot_product_visitor.h"
#include "data_tools/to_binary_visitor.h"
#include "data_tools/data_row.h"
#include "data_tools/data_row_factory.h"
#include "data_tools/base_data.h"
#include "representative_subset_calculator/timers/timers.h"
#include "representative_subset_calculator/kernel_matrix/relevance_calculator.h"
#include "representative_subset_calculator/kernel_matrix/relevance_calculator_factory.h"
#include "representative_subset_calculator/kernel_matrix/kernel_matrix.h"
#include "representative_subset_calculator/naive_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_representative_subset_calculator.h"
#include "representative_subset_calculator/fast_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_fast_representative_subset_calculator.h"

#include "representative_subset_calculator/buffers/buffer_builder.h"

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include <mpi.h>

#include "representative_subset_calculator/streaming/synchronous_queue.h"
#include "representative_subset_calculator/streaming/bucket.h"
#include "representative_subset_calculator/streaming/candidate_seed.h"
#include "representative_subset_calculator/streaming/bucket_titrator.h"
#include "representative_subset_calculator/streaming/candidate_consumer.h"
#include "representative_subset_calculator/streaming/rank_buffer.h"
#include "representative_subset_calculator/streaming/receiver_interface.h"
#include "representative_subset_calculator/streaming/greedy_streamer.h"
#include "representative_subset_calculator/streaming/mpi_streaming_classes.h"
#include "representative_subset_calculator/streaming/streaming_subset.h"
#include "representative_subset_calculator/streaming/mpi_receiver.h"
#include "representative_subset_calculator/orchestrator/mpi_orchestrator.h"

void randGreedi(
    const AppData &appData, 
    const SegmentedData &data, 
    const std::vector<unsigned int> &rowToRank, 
    Timers &timers
) {
    timers.totalCalculationTime.startTimer();
    std::unique_ptr<SubsetCalculator> calculator(MpiOrchestrator::getCalculator(appData));

    timers.localCalculationTime.startTimer();
    std::unique_ptr<Subset> localSolution(calculator->getApproximationSet(data, appData.outputSetSize));
    timers.localCalculationTime.stopTimer();

    // TODO: batch this into blocks using a custom MPI type to send higher volumes of data.
    timers.bufferEncodingTime.startTimer();
    std::vector<double> sendBuffer;
    unsigned int sendDataSize = BufferBuilder::buildSendBuffer(data, *localSolution.get(), sendBuffer);
    std::vector<int> receivingDataSizesBuffer(appData.worldSize, 0);
    timers.bufferEncodingTime.stopTimer();

    timers.communicationTime.startTimer();
    MPI_Gather(&sendDataSize, 1, MPI_INT, receivingDataSizesBuffer.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
    timers.communicationTime.stopTimer();

    timers.bufferEncodingTime.startTimer();
    std::vector<double> receiveBuffer;
    std::vector<int> displacements;
    if (appData.worldRank == 0) {
        BufferBuilder::buildReceiveBuffer(receivingDataSizesBuffer, receiveBuffer);
        BufferBuilder::buildDisplacementBuffer(receivingDataSizesBuffer, displacements);
    }
    timers.bufferEncodingTime.stopTimer();

    timers.communicationTime.startTimer();
    MPI_Gatherv(
        sendBuffer.data(), 
        sendBuffer.size(), 
        MPI_DOUBLE, 
        receiveBuffer.data(), 
        receivingDataSizesBuffer.data(), 
        displacements.data(),
        MPI_DOUBLE, 
        0, 
        MPI_COMM_WORLD
    );
    timers.communicationTime.stopTimer();

    if (appData.worldRank == 0) {
        std::unique_ptr<SubsetCalculator> globalCalculator(MpiOrchestrator::getCalculator(appData));
        std::unique_ptr<DataRowFactory> factory(Orchestrator::getDataRowFactory(appData));
        GlobalBufferLoader bufferLoader(receiveBuffer, data.totalColumns(), displacements, timers);
        std::unique_ptr<Subset> globalSolution(bufferLoader.getSolution(move(globalCalculator), appData.outputSetSize, *factory.get()));

        timers.totalCalculationTime.stopTimer();

        nlohmann::json result = MpiOrchestrator::buildMpiOutput(appData, *globalSolution.get(), data, timers, rowToRank);
        std::ofstream outputFile;
        outputFile.open(appData.outputFile);
        outputFile << result.dump(2);
        outputFile.close();
    } else {
        // used to load global timers on rank 0
        timers.totalCalculationTime.stopTimer();
        auto dummyResult = Subset::empty();

        MpiOrchestrator::buildMpiOutput(appData, *dummyResult.get(), data, timers, rowToRank);
    }
}

void streaming(
    const AppData &appData, 
    const SegmentedData &data, 
    const std::vector<unsigned int> &rowToRank, 
    Timers &timers
) {
    // This is hacky. We only do this because the receiver doesn't load any data and therefore
    //  doesn't know the rowsize of the input. Just load one row instead.
    unsigned int rowSize = data.totalColumns();
    std::vector<unsigned int> rowSizes(appData.worldSize);
    MPI_Gather(&rowSize, 1, MPI_INT, rowSizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (appData.worldRank == 0) {
        std::cout << "rank 0 entered into the streaming function and knows the total columns of "<< rowSize << std::endl;
        timers.totalCalculationTime.startTimer();

        // if you are using an adjacency list as input, double this. Sparse data could theoretically
        //  be double the size of dense data in worst case. Senders will not send more data than they
        //  have, but you need to be prepared for worst case in the receiver (allocation is cheaper
        //  than sends anyway).
        rowSize = appData.adjacencyListColumnCount > 0 ? rowSizes.back() : rowSizes.back() * 2;


        std::unique_ptr<DataRowFactory> factory(Orchestrator::getDataRowFactory(appData));
        std::unique_ptr<Receiver> receiver(
            MpiReceiver::buildReceiver(appData.worldSize, rowSize, *factory)
        );
        std::unique_ptr<CandidateConsumer> consumer(MpiOrchestrator::buildConsumer(
            appData, omp_get_num_threads() - 1, appData.worldSize - 1)
        );
        SeiveGreedyStreamer streamer(*receiver.get(), *consumer.get(), timers);

        std::cout << "rank 0 built all objects, ready to start receiving" << std::endl;
        std::unique_ptr<Subset> solution(streamer.resolveStream());
        timers.totalCalculationTime.stopTimer();
        std::cout << "rank 0 finished receiving" << std::endl;

        nlohmann::json result = MpiOrchestrator::buildMpiOutput(
            appData, *solution.get(), data, timers, rowToRank
        );
        std::ofstream outputFile;
        outputFile.open(appData.outputFile);
        outputFile << result.dump(2);
        outputFile.close();
        std::cout << "rank 0 outout all information" << std::endl;
    } else {
        std::cout << "rank " << appData.worldRank << " entered streaming function and know the total columns of " << data.totalColumns() << std::endl;
        timers.totalCalculationTime.startTimer();
        std::unique_ptr<MutableSubset> subset(new StreamingSubset(data, std::floor(appData.outputSetSize * appData.alpha), timers));
        std::unique_ptr<SubsetCalculator> calculator(MpiOrchestrator::getCalculator(appData));
        std::cout << "rank " << appData.worldRank << " ready to start streaming local seeds" << std::endl;

        timers.localCalculationTime.startTimer();
        std::unique_ptr<Subset> localSolution(calculator->getApproximationSet(move(subset), data, std::floor(appData.outputSetSize * appData.alpha)));
        std::cout << "rank " << appData.worldRank << " finished streaming local seeds" << std::endl;
        timers.localCalculationTime.stopTimer();
        timers.totalCalculationTime.stopTimer();

        auto dummyResult = Subset::empty();
        MpiOrchestrator::buildMpiOutput(appData, *dummyResult.get(), data, timers, rowToRank);
    }
}

int main(int argc, char** argv) {
    CLI::App app{"Approximates the best possible approximation set for the input dataset using MPI."};
    AppData appData;
    MpiOrchestrator::addMpiCmdOptions(app, appData);
    CLI11_PARSE(app, argc, argv);

    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &appData.worldRank);
    MPI_Comm_size(MPI_COMM_WORLD, &appData.worldSize);

    unsigned int seed = (unsigned int)time(0);
    MPI_Bcast(&seed, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);

    std::vector<unsigned int> rowToRank = MpiOrchestrator::getRowToRank(appData, seed);

    Timers timers;
    timers.loadingDatasetTime.startTimer();
    std::ifstream inputFile;
    inputFile.open(appData.loadInput.inputFile);
    std::unique_ptr<SegmentedData> data(Orchestrator::buildMpiData(appData, inputFile, rowToRank));
    inputFile.close();
    timers.loadingDatasetTime.stopTimer();

    timers.barrierTime.startTimer();
    MPI_Barrier(MPI_COMM_WORLD);
    timers.barrierTime.stopTimer();

    if (appData.distributedAlgorithm == 0) {
        randGreedi(appData, *data, rowToRank, timers);
    } else if (appData.distributedAlgorithm == 1 || appData.distributedAlgorithm == 2) {
        streaming(appData, *data, rowToRank, timers);
    } else {
        std::cout << "did not recognized distributedAlgorithm of " << appData.distributedAlgorithm << std::endl;
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}