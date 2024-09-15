#include "log_macros.h"

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
#include <thread>

void randGreedi(
    const AppData &appData, 
    const SegmentedData &data, 
    const std::vector<unsigned int> &rowToRank, 
    Timers &timers
) {
    unsigned int sendDataSize = 0;
    std::vector<int> receivingDataSizesBuffer(appData.worldSize, 0);
    std::vector<float> sendBuffer;
    timers.totalCalculationTime.startTimer();
    if (appData.worldRank != 0) {
        
        std::unique_ptr<SubsetCalculator> calculator(MpiOrchestrator::getCalculator(appData));
        
        timers.localCalculationTime.startTimer();
        std::unique_ptr<Subset> localSolution(calculator->getApproximationSet(data, appData.outputSetSize));
        timers.localCalculationTime.stopTimer();
        
        // TODO: batch this into blocks using a custom MPI type to send higher volumes of data.
        timers.bufferEncodingTime.startTimer();
        sendDataSize = BufferBuilder::buildSendBuffer(data, *localSolution.get(), sendBuffer);
        timers.bufferEncodingTime.stopTimer();
    } 
    
    timers.communicationTime.startTimer();
    MPI_Gather(&sendDataSize, 1, MPI_INT, receivingDataSizesBuffer.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
    timers.communicationTime.stopTimer();
    
    timers.bufferEncodingTime.startTimer();
    std::vector<float> receiveBuffer;
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
        MPI_FLOAT, 
        receiveBuffer.data(), 
        receivingDataSizesBuffer.data(), 
        displacements.data(),
        MPI_FLOAT, 
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
        rowSize = rowSizes.back();
        spdlog::info("rank 0 entered into the streaming function and knows the total columns of {0:d}", rowSize);
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
        SeiveGreedyStreamer streamer(*receiver.get(), *consumer.get(), timers, !appData.stopEarly);

        spdlog::info("rank 0 built all objects, ready to start receiving");
        std::unique_ptr<Subset> solution(streamer.resolveStream());
        timers.totalCalculationTime.stopTimer();
        spdlog::info("rank 0 finished receiving");

        MPI_Barrier(MPI_COMM_WORLD);
        spdlog::info("receiver is through the barrier");

        nlohmann::json result = MpiOrchestrator::buildMpiOutput(
            appData, *solution.get(), data, timers, rowToRank
        );
        std::ofstream outputFile;
        outputFile.open(appData.outputFile);
        outputFile << result.dump(2);
        outputFile.close();
        spdlog::info("rank 0 output all information");
    } else {
        spdlog::info("rank {0:d} entered streaming function and know the total columns of {1:d}", appData.worldRank, data.totalColumns());
        timers.totalCalculationTime.startTimer();
        
        std::unique_ptr<MutableSubset> subset(new StreamingSubset(data, std::floor(appData.outputSetSize * appData.alpha), timers));
        std::unique_ptr<SubsetCalculator> calculator(MpiOrchestrator::getCalculator(appData));
        spdlog::info("rank {0:d} is ready to start streaming local seeds", appData.worldRank);

        timers.localCalculationTime.startTimer();
        std::thread findAndSendSolution([&calculator, &subset, &data, &appData]() {
            return calculator->getApproximationSet(move(subset), data, std::floor(appData.outputSetSize * appData.alpha));
        });

        // Block until reciever is finished.
        MPI_Barrier(MPI_COMM_WORLD);
        if (appData.stopEarly) {
            findAndSendSolution.detach();
        } else {
            // Don't kill any extra sends, we need them to compare local solutions to the global solution.
            findAndSendSolution.join();
        }
        spdlog::info("rank {0:d} finished streaming local seeds", appData.worldRank);

        timers.localCalculationTime.stopTimer();
        timers.totalCalculationTime.stopTimer();

        auto dummyResult = Subset::empty();
        MpiOrchestrator::buildMpiOutput(appData, *dummyResult.get(), data, timers, rowToRank);
    }
}

int main(int argc, char** argv) {

    LoggerHelper::setupLoggers();

    CLI::App app{"Approximates the best possible approximation set for the input dataset using MPI."};
    AppData appData;
    MpiOrchestrator::addMpiCmdOptions(app, appData);
    CLI11_PARSE(app, argc, argv);

    if (appData.numberOfDataRows == 0) {
        throw std::invalid_argument("Please set the number of rows (the numberOfDataRows arg)");
    }

    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &appData.worldRank);
    MPI_Comm_size(MPI_COMM_WORLD, &appData.worldSize);

    unsigned int seed = (unsigned int)time(0);
    MPI_Bcast(&seed, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);

    std::vector<unsigned int> rowToRank = MpiOrchestrator::getRowToRank(appData, seed);

    Timers timers;
    std::vector<Timers> comparisonTimers(3);

    for (auto t: comparisonTimers)
        t.loadingDatasetTime.startTimer();
    timers.loadingDatasetTime.startTimer();

    // All of this is duplicated between files, you can pretty easily build another class
    //  to contain this so you don't repeat yourself.
    
    // Put this somewhere more sane
    const unsigned int DEFAULT_VALUE = -1;

    std::unique_ptr<SegmentedData> data;
    if (appData.loadInput.inputFile != EMPTY_STRING) {
        std::ifstream inputFile;
        inputFile.open(appData.loadInput.inputFile);
        std::unique_ptr<LineFactory> getter(std::unique_ptr<FromFileLineFactory>(new FromFileLineFactory(inputFile)));
        data = Orchestrator::buildMpiData(appData, *getter.get(), rowToRank);
        inputFile.close();
    } else if (appData.generateInput.seed != DEFAULT_VALUE) {
        std::unique_ptr<GeneratedLineFactory> getter(Orchestrator::getLineGenerator(appData));
        data = Orchestrator::buildMpiData(appData, *getter.get(), rowToRank);
    }

    for (auto t: comparisonTimers)
        t.loadingDatasetTime.stopTimer();
    timers.loadingDatasetTime.stopTimer();

    for (auto t: comparisonTimers)
        t.barrierTime.startTimer();
    timers.barrierTime.startTimer();

    MPI_Barrier(MPI_COMM_WORLD);

    for (auto t: comparisonTimers)
        t.barrierTime.stopTimer();
    timers.barrierTime.stopTimer();

    if (appData.distributedAlgorithm == 0) {
        randGreedi(appData, *data, rowToRank, timers);
    } else if (appData.distributedAlgorithm == 1 || appData.distributedAlgorithm == 2) {
        streaming(appData, *data, rowToRank, timers);
    } else if (appData.distributedAlgorithm == 3) {

        std::string output = appData.outputFile;
        spdlog::info("Starting Baseline RandGreedI...");
        appData.distributedAlgorithm = 0;
        appData.outputFile = output + "_RandGreedI_Base.json";
        randGreedi(appData, *data, rowToRank, comparisonTimers[0]);

        spdlog::info("Done! Starting RandGreedI + Streaming with Sieve Streaming...");

        appData.distributedAlgorithm = 1;
        appData.outputFile = output + "_RandGreedI_Sieve.json";
        streaming(appData, *data, rowToRank, comparisonTimers[1]);

        spdlog::info("Done! Starting RandGreedI + Streaming with Three-Sieves Streaming... ");

        appData.distributedAlgorithm = 2;
        appData.outputFile = output + "_RandGreedI_ThreeSieve.json";
        streaming(appData, *data, rowToRank, comparisonTimers[2]);
        
        spdlog::info("Done!");

    } else {
        spdlog::error("did not recognize distributed Algorithm of {0:d}", appData.distributedAlgorithm);
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
