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
#include "representative_subset_calculator/sendAll_subset_calculator.h"

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


void streaming(
    const AppData &appData, 
    const SegmentedData &data, 
    const std::vector<unsigned int> &rowToRank, 
    Timers &timers
) {
    unsigned int rowSize = data.totalColumns();
    std::vector<unsigned int> rowSizes(appData.worldSize);
    MPI_Gather(&rowSize, 1, MPI_INT, rowSizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (appData.worldRank == 0) {
        rowSize = rowSizes.back();
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
        std::unique_ptr<Receiver> zeroReceiver(new ZeroMarginalReceiver(move(receiver)));

        std::unique_ptr<CandidateConsumer> consumer(new StreamingCandidateConsumer(
            MpiOrchestrator::buildTitrator(appData, omp_get_num_threads() - 1))
        );
        
        // Stop early still not implemented.
        SeiveGreedyStreamer streamer(*zeroReceiver.get(), *consumer.get(), timers, !appData.stopEarly);

        std::cout << "rank 0 built all objects, ready to start receiving" << std::endl;
        std::unique_ptr<Subset> solution(streamer.resolveStream());
        timers.totalCalculationTime.stopTimer();

        std::cout << "rank 0 finished receiving" << std::endl;
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "receiver is through the barrier" << std::endl;

        nlohmann::json result = MpiOrchestrator::buildMpiOutput(
            appData, *solution.get(), data, timers, rowToRank
        );
        std::ofstream outputFile;
        outputFile.open(appData.outputFile);
        outputFile << result.dump(2);
        outputFile.close();
        std::cout << "rank 0 outout all information" << std::endl;

    } else {
        // We can load/generate our datasets in parallel here. This will increase speed dramatically.
        std::unique_ptr<MutableSubset> subset(new StreamingSubset(data, appData.numberOfDataRows, timers));
        std::unique_ptr<SubsetCalculator> calculator(MpiOrchestrator::getCalculator(appData));

        std::thread findAndSendSolution([&calculator, &subset, &data, &appData]() {
            calculator->getApproximationSet(move(subset), data, appData.numberOfDataRows);
        });

        MPI_Barrier(MPI_COMM_WORLD);
        if (appData.stopEarly) {
            findAndSendSolution.detach();
        } else {
            findAndSendSolution.join();
        }

        std::cout << "rank " << appData.worldRank << " finished streaming local seeds" << std::endl;
        auto dummyResult = Subset::empty();
        MpiOrchestrator::buildMpiOutput(appData, *dummyResult.get(), data, timers, rowToRank);
    }
}

int main(int argc, char** argv) {
    CLI::App app{"Approximates the best possible approximation set for the input dataset using streaming."};
    AppData appData;
    MpiOrchestrator::addMpiCmdOptions(app, appData);
    CLI11_PARSE(app, argc, argv);

    if (appData.numberOfDataRows == 0) {
        throw std::invalid_argument("Please set the number of rows (the numberOfDataRows arg)");
    }

    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &appData.worldRank);
    MPI_Comm_size(MPI_COMM_WORLD, &appData.worldSize);

    std::cout << "World Size: " << appData.worldSize << std::endl;
    if (appData.worldSize != 2) {
        throw std::invalid_argument("Streaming only uses 2 ranks -- one streamer and one receiver");
    }

    // Loads dataset only on rank 1
    std::vector<unsigned int> rowToRank(appData.numberOfDataRows, 1); 

    Timers timers;
    
    timers.loadingDatasetTime.startTimer();

    // Put this somewhere more sane
    const unsigned int DEFAULT_VALUE = -1;

    std::unique_ptr<LineFactory> getter;
    std::ifstream inputFile;
    if (appData.loadInput.inputFile != EMPTY_STRING) {
        inputFile.open(appData.loadInput.inputFile);
        getter = std::unique_ptr<FromFileLineFactory>(new FromFileLineFactory(inputFile));
    } else if (appData.generateInput.seed != DEFAULT_VALUE) {
        getter = Orchestrator::getLineGenerator(appData);
    }

    std::unique_ptr<SegmentedData> data(Orchestrator::buildMpiData(appData, *getter.get(), rowToRank));
    if (appData.loadInput.inputFile != EMPTY_STRING) {
        inputFile.close();
    } 

    timers.loadingDatasetTime.stopTimer();

    timers.barrierTime.startTimer();
    MPI_Barrier(MPI_COMM_WORLD);
    timers.barrierTime.stopTimer();
    
    streaming(appData, *data, rowToRank, timers);

    MPI_Finalize();
    return EXIT_SUCCESS;
}