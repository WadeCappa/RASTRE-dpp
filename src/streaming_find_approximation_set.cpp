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
    LineFactory &source, 
    const std::vector<unsigned int> &rowToRank, 
    Timers &timers
) {
    if (appData.adjacencyListColumnCount <= 0) {
        throw std::invalid_argument("Even with dense datasets, the number of columns is reqruied. This is because standalone streaming loads the dataset at runtime.");
    }
    unsigned int rowSize = appData.adjacencyListColumnCount;

    if (appData.worldRank == 0) {
        std::cout << "rank 0 entered into the streaming function and knows the total columns of "<< rowSize << std::endl;
        timers.totalCalculationTime.startTimer();

        // if you are using an adjacency list as input, double this. Sparse data could theoretically
        //  be double the size of dense data in worst case. Senders will not send more data than they
        //  have, but you need to be prepared for worst case in the receiver (allocation is cheaper
        //  than sends anyway).
        rowSize *= 2;

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

        // This is used only so that we have something to provide the mpi output.
        auto dummyData(FullyLoadedData::load(std::vector<std::vector<float>>()));

        nlohmann::json result = MpiOrchestrator::buildMpiOutput(
            appData, *solution.get(), *dummyData, timers, rowToRank
        );
        std::ofstream outputFile;
        outputFile.open(appData.outputFile);
        outputFile << result.dump(2);
        outputFile.close();
        std::cout << "rank 0 outout all information" << std::endl;

    } else {
        // We can load/generate our datasets in parallel here. This will increase speed dramatically.
        std::thread findAndSendSolution([&source, &appData]() {
            std::vector<std::unique_ptr<MpiSendRequest>> sends;
            size_t currentRow = 0;
            std::unique_ptr<DataRowFactory> factory(Orchestrator::getDataRowFactory(appData));
            while (true) {
                std::unique_ptr<DataRow> nextRow(factory->maybeGet(source));
                if (nextRow == nullptr) {
                    std::cout << "reached end of data" << std::endl;
                    break;
                }

                ToBinaryVisitor visitor;
                std::vector<float> rowToSend(move(nextRow->visit(visitor)));

                // second to last value should be the marginal gain of this element for the local solution.
                rowToSend.push_back(0.0);

                // last value should be the global row index
                rowToSend.push_back(currentRow++);

                // Mark the end of the send buffer
                rowToSend.push_back(CommunicationConstants::endOfSendTag());

                sends.push_back(std::unique_ptr<MpiSendRequest>(new MpiSendRequest(move(rowToSend))));

                // Keep at least one seed until finalize is called. Otherwise there is no garuntee of
                //  how many seeds there are left to find.
                if (sends.size() > 1) {
                    // Tag should be -1 iff this is the last seed to be sent. This code purposefully
                    //  does not send the last seed until finalize is called
                    const int tag = CommunicationConstants::getContinueTag();
                    sends[sends.size() - 2]->isend(tag);
                }
            }

            for (size_t i = 0; i < sends.size(); i++) {
                sends[i]->waitForISend();
            }
        });

        MPI_Barrier(MPI_COMM_WORLD);
        if (appData.stopEarly) {
            findAndSendSolution.detach();
        } else {
            findAndSendSolution.join();
        }

        std::cout << "rank " << appData.worldRank << " finished streaming local seeds" << std::endl;
        auto dummyResult = Subset::empty();
        auto dummyData(FullyLoadedData::load(std::vector<std::vector<float>>()));
        MpiOrchestrator::buildMpiOutput(appData, *dummyResult.get(), *dummyData, timers, rowToRank);
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
    
    // Data is loaded/generated during this method
    streaming(appData, *getter.get(), rowToRank, timers);

    if (appData.loadInput.inputFile != EMPTY_STRING) {
        inputFile.close();
    } 

    MPI_Finalize();
    return EXIT_SUCCESS;
}