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
#include "representative_subset_calculator/streaming/synchronous_queue.h"
#include "representative_subset_calculator/streaming/bucket.h"
#include "representative_subset_calculator/streaming/candidate_seed.h"
#include "representative_subset_calculator/streaming/bucket_titrator.h"
#include "representative_subset_calculator/streaming/candidate_consumer.h"
#include "representative_subset_calculator/orchestrator/mpi_orchestrator.h"

#include "representative_subset_calculator/buffers/buffer_builder.h"
#include "representative_subset_calculator/memoryProfiler/MemUsage.h"

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <random>
#include <algorithm>

int main(int argc, char** argv) {
    CLI::App app{"Approximates the best possible approximation set for the input dataset using streaming."};
    AppData appData;
    MpiOrchestrator::addMpiCmdOptions(app, appData);
    CLI11_PARSE(app, argc, argv);

    Timers timers;
    size_t memUsage;
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

    SynchronousQueue<std::unique_ptr<CandidateSeed>> queue;
    std::vector<std::unique_ptr<CandidateSeed>> elements;
    std::unique_ptr<DataRowFactory> factory(Orchestrator::getDataRowFactory(appData));

    timers.loadingDatasetTime.startTimer();
    size_t globalRow = 0;

    while (true) {
        std::unique_ptr<DataRow> nextRow(factory->maybeGet(*getter));

        if (nextRow == nullptr) {
            break;
        }

        auto element = std::unique_ptr<CandidateSeed>(new CandidateSeed(globalRow++, move(nextRow), 1));
        elements.push_back(move(element));
    }
    timers.loadingDatasetTime.stopTimer();

    // Randomize Order
    std::random_device rd; 
    std::mt19937 g(rd()); 
    std::shuffle(elements.begin(), elements.end(), g);
    for (auto& element : elements) {
        queue.push(move(element)); // Move elements into the queue
    }        

    timers.totalCalculationTime.startTimer();

    auto baseline = getPeakRSS();

    std::unique_ptr<CandidateConsumer> candidateConsumer(new StreamingCandidateConsumer(
        MpiOrchestrator::buildTitrator(appData, omp_get_num_threads() - 1))
    );

    candidateConsumer->accept(queue, timers);
    
    memUsage = getPeakRSS()- baseline;
    
    timers.totalCalculationTime.stopTimer();

    std::unique_ptr<Subset> solution(candidateConsumer->getBestSolutionDestroyConsumer());
    
    if (appData.loadInput.inputFile != EMPTY_STRING) {
        inputFile.close();
    } 
    
    std::vector<std::unique_ptr<DataRow>> dummyData;
    std::vector<size_t> dummyRowMapping;
    size_t dummyColumns = 0;  // A placeholder for columns, set to 0 or any valid number.

    SegmentedData dummySegmentedData(std::move(dummyData), std::move(dummyRowMapping), dummyColumns);

    nlohmann::json result = Orchestrator::buildOutput(appData, *solution.get(), dummySegmentedData, timers);
    result.push_back({"Memory (KiB)", memUsage});
    std::ofstream outputFile;
    outputFile.open(appData.outputFile);
    outputFile << result.dump(2);
    outputFile.close();

    return 0;
}