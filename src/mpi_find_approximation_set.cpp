#include "log_macros.h"

#include "user_mode/user_data.h"
#include "representative_subset_calculator/streaming/communication_constants.h"
#include "representative_subset_calculator/representative_subset.h"
#include "data_tools/data_row_visitor.h"
#include "data_tools/dot_product_visitor.h"
#include "data_tools/to_binary_visitor.h"
#include "data_tools/data_row.h"
#include "data_tools/data_row_factory.h"
#include "data_tools/base_data.h"
#include "representative_subset_calculator/timers/timers.h"
#include "data_tools/user_mode_data.h"
#include "representative_subset_calculator/kernel_matrix/relevance_calculator.h"
#include "representative_subset_calculator/kernel_matrix/relevance_calculator_factory.h"
#include "representative_subset_calculator/kernel_matrix/kernel_matrix.h"
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
#include "representative_subset_calculator/streaming/naive_receiver.h"
#include "representative_subset_calculator/streaming/mpi_receiver.h"
#include "representative_subset_calculator/orchestrator/mpi_orchestrator.h"
#include "representative_subset_calculator/memoryProfiler/MemUsage.h"
#include "user_mode/user_score.h"
#include "user_mode/user_subset.h"

#include <thread>

std::unique_ptr<Subset> randGreedi(
    const AppData &appData, 
    const BaseData &data, 
    const RelevanceCalculatorFactory& calcFactory,
    Timers &timers
) {
    unsigned int sendDataSize = 0;
    std::vector<int> receivingDataSizesBuffer(appData.worldSize, 0);
    std::vector<float> sendBuffer;
    timers.totalCalculationTime.startTimer();
    if (appData.worldRank != 0) {
        spdlog::info("attempting to calculate global solution on rank {0:d}", appData.worldRank);
        std::unique_ptr<SubsetCalculator> calculator(MpiOrchestrator::getCalculator(appData));
        
        std::unique_ptr<RelevanceCalculator> calc(calcFactory.build(data));
        timers.localCalculationTime.startTimer();
        std::unique_ptr<Subset> localSolution(calculator->getApproximationSet(
            NaiveMutableSubset::makeNew(), *calc, data, appData.outputSetSize)
        );
        timers.localCalculationTime.stopTimer();
        spdlog::info("finished finding solution for rank {0:d} of score {1:f}", appData.worldRank, localSolution->getScore());
        
        // TODO: batch this into blocks using a custom MPI type to send higher volumes of data.
        timers.bufferEncodingTime.startTimer();
        sendDataSize = BufferBuilder::buildSendBuffer(data, *localSolution.get(), sendBuffer);
        timers.bufferEncodingTime.stopTimer();
    } 
    
    spdlog::debug("starting gather on rank {0:d}, sending {1:d} with buffer size {2:d}", appData.worldRank, sendDataSize, sendBuffer.size());
    timers.communicationTime.startTimer();
    MPI_Gather(&sendDataSize, 1, MPI_UNSIGNED, receivingDataSizesBuffer.data(), 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
    timers.communicationTime.stopTimer();

    MPI_Gather(&sendDataSize, 1, MPI_UNSIGNED, receivingDataSizesBuffer.data(), 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
    for (size_t i = 0; i < receivingDataSizesBuffer.size(); i++) {
        spdlog::debug("rank {0:d} sees {1:d} from {2:d}", appData.worldRank, receivingDataSizesBuffer[i], i);
    }
    
    spdlog::debug("building buffer rank {0:d}", appData.worldRank);
    timers.bufferEncodingTime.startTimer();
    std::vector<float> receiveBuffer;
    std::vector<int> displacements;
    if (appData.worldRank == 0) {
        BufferBuilder::buildReceiveBuffer(receivingDataSizesBuffer, receiveBuffer);
        BufferBuilder::buildDisplacementBuffer(receivingDataSizesBuffer, displacements);
    }
    timers.bufferEncodingTime.stopTimer();
    
    timers.communicationTime.startTimer();
    spdlog::debug("gather v rank {0:d} sending {1:d} bytes", appData.worldRank, sendBuffer.size());

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
        spdlog::debug("rank 0 starting to process seeds");
        std::unique_ptr<SubsetCalculator> globalCalculator(MpiOrchestrator::getCalculator(appData));
        std::unique_ptr<DataRowFactory> factory(Orchestrator::getDataRowFactory(appData));

        spdlog::debug("building buffer on rank 0");
        GlobalBufferLoader bufferLoader(receiveBuffer, data.totalColumns(), displacements, timers, calcFactory);

        spdlog::debug("getting global solution");
        std::unique_ptr<Subset> globalSolution(bufferLoader.getSolution(move(globalCalculator), appData.outputSetSize, *factory.get()));

        timers.totalCalculationTime.stopTimer();

        spdlog::debug("rank 0 returning solution");
        return move(globalSolution);
    } else {
        // used to load global timers on rank 0
        timers.totalCalculationTime.stopTimer();
        return Subset::empty();
    }
}

std::unique_ptr<Subset> streaming(
    const AppData &appData, 
    const BaseData &data, 
    RelevanceCalculatorFactory& calcFactory,
    Timers &timers
) {
    // This is hacky. We only do this because the receiver doesn't load any data and therefore
    //  doesn't know the rowsize of the input. Just load one row instead.
    unsigned int rowSize = appData.worldRank == 0 ? 0 : data.totalColumns();
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
            appData, omp_get_num_threads() - 1, appData.worldSize - 1, calcFactory)
        );
        SeiveGreedyStreamer streamer(*receiver.get(), *consumer.get(), timers, !appData.stopEarly);

        spdlog::info("rank 0 built all objects, ready to start receiving");
        std::unique_ptr<Subset> solution(streamer.resolveStream());
        timers.totalCalculationTime.stopTimer();
        spdlog::info("rank 0 finished receiving");

        MPI_Barrier(MPI_COMM_WORLD);

        return move(solution);
    } else {
        spdlog::info("rank {0:d} entered streaming function and know the total columns of {1:d}", appData.worldRank, data.totalColumns());
        timers.totalCalculationTime.startTimer();
        
        std::unique_ptr<MutableSubset> subset(
            new StreamingSubset(data, NaiveMutableSubset::makeNew(), timers, std::floor(appData.outputSetSize * appData.alpha))
        );
        std::unique_ptr<SubsetCalculator> calculator(MpiOrchestrator::getCalculator(appData));
        spdlog::info("rank {0:d} is ready to start streaming local seeds", appData.worldRank);

        timers.localCalculationTime.startTimer();

        std::unique_ptr<RelevanceCalculator> calc(calcFactory.build(data));
        std::unique_ptr<Subset> localSolution(calculator->getApproximationSet(move(subset), *calc, data, appData.outputSetSize));

        spdlog::info("rank {0:d} finished streaming local seeds. Found {1:d} seeds of score {2:f}", appData.worldRank, localSolution->size(), localSolution->getScore());

        timers.localCalculationTime.stopTimer();
        timers.totalCalculationTime.stopTimer();

        MPI_Barrier(MPI_COMM_WORLD);

        return Subset::empty();
    }
}

std::vector<std::unique_ptr<Subset>> getSolutions(
    AppData& appData, // ideally this should be const
    const BaseData &data, 
    RelevanceCalculatorFactory& calc,
    Timers &timers,
    std::vector<Timers> comparisonTimers
) {
    std::vector<std::unique_ptr<Subset>> solutions;
    if (appData.distributedAlgorithm == 0) {
        solutions.push_back(randGreedi(appData, data, calc, timers));
    } else if (appData.distributedAlgorithm == 1 || appData.distributedAlgorithm == 2) {
        solutions.push_back(streaming(appData, data, calc, timers));
    } else if (appData.distributedAlgorithm == 3) {

        spdlog::error("currently not supported");
        exit(1);
    }

        /**

        std::string output = appData.outputFile;
        spdlog::info("Starting Baseline RandGreedI...");
        appData.distributedAlgorithm = 0;
        // need to mark output with "_RandGreedI_Base.json";
        randGreedi(appData, data, rowToRank, user, comparisonTimers[0]);

        spdlog::info("Done! Starting RandGreedI + Streaming with Sieve Streaming...");

        appData.distributedAlgorithm = 1;
        // need to mark output with "_RandGreedI_Sieve.json";
        streaming(appData, data, rowToRank, user, comparisonTimers[1]);

        spdlog::info("Done! Starting RandGreedI + Streaming with Three-Sieves Streaming... ");

        appData.distributedAlgorithm = 2;
        // need to mark output with "_RandGreedI_ThreeSieve.json";
        streaming(appData, data, rowToRank, user, comparisonTimers[2]);
        
        spdlog::info("Done!");
    } else {
        spdlog::error("did not recognize distributed Algorithm of {0:d}", appData.distributedAlgorithm);
    }
         * 
         */

    return move(solutions);
}


int main(int argc, char** argv) {

    size_t baselinePreLoad = getPeakRSS();

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

    if (appData.sendAllToReceiver && appData.worldRank == 0) {
        spdlog::warn("sendAllToReceiver is an experimental feature and has not been fully implemented. Use with caution. Most critically, this feature does not support loading while sending and will load the entire dataset in bulk before sending any seeds. Depending on your input, this might be very expensive.");
    }

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

    size_t memUsagePreLoad = getPeakRSS() - baselinePreLoad;
    spdlog::debug("rank {0:d} allocated {1:d} KiB during the preload", appData.worldRank, memUsagePreLoad);

    size_t baseline = getPeakRSS();

    spdlog::info("Starting load for rank {0:d}", appData.worldRank);
    std::unique_ptr<BaseData> data;
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

    size_t memUsage = getPeakRSS() - baseline;

    spdlog::debug("rank {0:d} allocated {1:d} KiB for the dataset", appData.worldRank, memUsage);

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

    std::vector<std::unique_ptr<UserData>> userData;
    if (appData.userModeFile != EMPTY_STRING) {
        // load all user-data for the global node since we don't know what we'll be evaluating when 
        // data ends up on node 0
        userData = appData.worldRank != 0 ? 
            UserDataImplementation::loadForMultiMachineMode(appData.userModeFile, rowToRank, appData.worldRank) : 
            UserDataImplementation::load(appData.userModeFile);
        spdlog::info("Finished loading user data for {0:d} users ...", userData.size());
    }

    for (size_t i = 0; i < userData.size(); i++) {
        spdlog::debug("user {0:d} has cu size {1:d}, ru size {2:d}, and test of {3:d}", userData[i]->getUserId(), userData[i]->getRu().size(), userData[i]->getCu().size(), userData[i]->getTestId());
    }

    std::vector<std::unique_ptr<Subset>> solutions;
    if (userData.size() == 0) {
        std::unique_ptr<RelevanceCalculatorFactory> calcFactory(new NaiveRelevanceCalculatorFactory());
        solutions = getSolutions(appData, *data, *calcFactory, timers, comparisonTimers);
    } else {
        for (const auto & user : userData) {
            std::unique_ptr<UserModeDataDecorator> userModeDataDecorator(UserModeDataDecorator::create(*data, *user.get()));
            spdlog::info("rank {0:d} starting to process user {1:d} of size {2:d} (original user size of {3:d})", 
                appData.worldRank, user->getUserId(), userModeDataDecorator->totalRows(), user->getCu().size());

            std::unique_ptr<RelevanceCalculatorFactory> calcFactory (
                new UserModeNaiveRelevanceCalculatorFactory(*user, appData.theta)
            );
            std::vector<std::unique_ptr<Subset>> new_solutions(
                getSolutions(
                    appData, 
                    *userModeDataDecorator, 
                    *calcFactory, 
                    timers, 
                    comparisonTimers
                )
            );
            spdlog::info("finished getting {0:d} solutions", new_solutions.size());
            for (size_t i = 0; i < new_solutions.size(); i++) {
                solutions.push_back(UserOutputInformationSubset::create(move(new_solutions[i]), *user));
            }
        }
    }

    nlohmann::json result = MpiOrchestrator::buildMpiOutput(appData, solutions, *data, timers, rowToRank);
    if (appData.worldRank == 0) {
        std::ofstream outputFile;
        outputFile.open(appData.outputFile);
        outputFile << result.dump(2);
        outputFile.close();
        spdlog::info("rank 0 output all information");
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
