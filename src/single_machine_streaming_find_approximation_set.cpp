
#include "spdlog/spdlog.h"
#include <CLI/CLI.hpp>
#include <thread>
#include <nlohmann/json.hpp>

#include "log_macros.h"
#include "representative_subset_calculator/representative_subset.h"
#include "representative_subset_calculator/orchestrator/orchestrator.h"
#include "representative_subset_calculator/orchestrator/mpi_orchestrator.h"
#include "user_mode/user_data.h"
#include "representative_subset_calculator/timers/timers.h"
#include "data_tools/data_row_factory.h"
#include "representative_subset_calculator/memoryProfiler/MemUsage.h"
#include "representative_subset_calculator/kernel_matrix/relevance_calculator_factory.h"
#include "representative_subset_calculator/streaming/bucket_titrator.h"
#include "representative_subset_calculator/streaming/candidate_consumer.h"
#include "representative_subset_calculator/streaming/receiver_interface.h"
#include "representative_subset_calculator/streaming/loading_receiver.h"
#include "representative_subset_calculator/streaming/greedy_streamer.h"
#include "user_mode/user_subset.h"

// Put this somewhere more sane
const unsigned int DEFAULT_VALUE = -1;

std::pair<std::unique_ptr<Subset>, size_t> loadWhileCalculating(
    const AppData& appData, 
    const std::optional<UserData*> user,
    Timers& timers
) { 
    std::unique_ptr<DataRowFactory> factory(Orchestrator::getDataRowFactory(appData));

    std::unique_ptr<LineFactory> getter;
    std::ifstream inputFile;
    if (appData.loadInput.inputFile != NO_FILE_DEFAULT) {
        inputFile.open(appData.loadInput.inputFile);
        getter = std::unique_ptr<FromFileLineFactory>(new FromFileLineFactory(inputFile));
    } else if (appData.generateInput.seed != DEFAULT_VALUE) {
        getter = Orchestrator::getLineGenerator(appData);
    }

    timers.totalCalculationTime.startTimer();

    auto baseline = getPeakRSS();

    std::unique_ptr<RelevanceCalculatorFactory> calcFactory(new NaiveRelevanceCalculatorFactory()); 
    if (user.has_value()) {
        calcFactory = std::unique_ptr<RelevanceCalculatorFactory>(
            new UserModeNaiveRelevanceCalculatorFactory(*user.value(), appData.theta)
        );  
    }

    std::unique_ptr<BucketTitrator> titrator(
        MpiOrchestrator::buildTitratorFactory(appData, omp_get_num_threads() - 1, *calcFactory)->createWithDynamicBuckets()
    );
    std::unique_ptr<NaiveCandidateConsumer> consumer(new NaiveCandidateConsumer(std::move(titrator), 1));

    std::unique_ptr<Receiver> receiver(new LoadingReceiver(std::move(factory), std::move(getter)));

    if (user.has_value()) {
        std::unique_ptr<Receiver> usermode_receiver(UserModeReceiver::create(std::move(receiver), *user.value()));
        receiver = std::move(usermode_receiver);
    }

    SeiveGreedyStreamer streamer(*receiver, *consumer.get(), timers, !appData.stopEarly);

    spdlog::info("Starting to load and stream");
    std::unique_ptr<Subset> solution(streamer.resolveStream());

    size_t memUsage = getPeakRSS()- baseline;

    if (appData.loadInput.inputFile != NO_FILE_DEFAULT) {
        inputFile.close();
    } 
    
    timers.totalCalculationTime.stopTimer();

    return std::make_pair(std::move(solution), memUsage);
}

std::pair<std::unique_ptr<Subset>, size_t> loadThenCalculate(
    const AppData& appData, 
    const std::optional<UserData*> user,
    Timers& timers
) {
    std::unique_ptr<DataRowFactory> factory(Orchestrator::getDataRowFactory(appData));

    std::unique_ptr<LineFactory> getter;
    std::ifstream inputFile;
    if (appData.loadInput.inputFile != NO_FILE_DEFAULT) {
        inputFile.open(appData.loadInput.inputFile);
        getter = std::unique_ptr<FromFileLineFactory>(new FromFileLineFactory(inputFile));
    } else if (appData.generateInput.seed != DEFAULT_VALUE) {
        getter = Orchestrator::getLineGenerator(appData);
    }

    SynchronousQueue<std::unique_ptr<CandidateSeed>> queue;
    std::vector<std::unique_ptr<CandidateSeed>> elements;

    timers.loadingDatasetTime.startTimer();

    auto loadBaseline = getPeakRSS();
    size_t globalRow = 0;

    std::unordered_set<unsigned long long> user_set;
    if (user.has_value()) {
        user_set = std::unordered_set<unsigned long long>(
            user.value()->getCu().begin(),
            user.value()->getCu().end());
    }

    while (true) {
        std::unique_ptr<DataRow> nextRow(factory->maybeGet(*getter));

        if (nextRow == nullptr) {
            break;
        }

        if (user.has_value() && user_set.find(globalRow) == user_set.end()) {
            globalRow++;
            continue;
        }

        auto element = std::unique_ptr<CandidateSeed>(new CandidateSeed(globalRow, std::move(nextRow), 1));
        elements.push_back(std::move(element));

        globalRow++;
    }
    timers.loadingDatasetTime.stopTimer();
    size_t memUsageOnLoad = getPeakRSS()- loadBaseline;

    if (appData.loadInput.inputFile != NO_FILE_DEFAULT) {
        inputFile.close();
    } 
    
    spdlog::info("Finished loading dataset of size {0:d} requiring {1:d} kB...", elements.size(), memUsageOnLoad);

    // Randomize Order
    std::random_device rd; 
    std::mt19937 g(rd()); 
    std::shuffle(elements.begin(), elements.end(), g);
    for (auto& element : elements) {
        queue.push(std::move(element)); // Move elements into the queue
    }        

    spdlog::info("Randomized dataset...");

    timers.totalCalculationTime.startTimer();

    auto baseline = getPeakRSS();

    std::unique_ptr<RelevanceCalculatorFactory> calcFactory(new NaiveRelevanceCalculatorFactory()); 
    if (user.has_value()) {
        calcFactory = std::unique_ptr<RelevanceCalculatorFactory>(
            new UserModeNaiveRelevanceCalculatorFactory(*user.value(), appData.theta)
        );  
    }
    std::unique_ptr<BucketTitrator> titrator(
        MpiOrchestrator::buildTitratorFactory(appData, omp_get_num_threads() - 1, *calcFactory)->createWithDynamicBuckets()
    );

    timers.insertSeedsTimer.startTimer();
    titrator->processQueue(queue);
    timers.insertSeedsTimer.stopTimer();
    
    size_t memUsage = getPeakRSS()- baseline;
    
    timers.totalCalculationTime.stopTimer();
    spdlog::info("mem usage of {0:d}", memUsage);
    std::unique_ptr<Subset> solution(titrator->getBestSolutionDestroyTitrator());
    return std::make_pair(std::move(solution), memUsage);
}

int main(int argc, char** argv) {
    LoggerHelper::setupLoggers();
    CLI::App app{"Approximates the best possible approximation set for the input dataset using streaming."};
    AppData appData;
    MpiOrchestrator::addMpiCmdOptions(app, appData);
    CLI11_PARSE(app, argc, argv);

    Timers timers;

    spdlog::info("Starting standalone streaming...");

    std::vector<std::unique_ptr<UserData>> userData;
    if (appData.userModeFile != NO_FILE_DEFAULT) {
        userData = UserDataImplementation::load(appData.userModeFile);
        spdlog::info("Finished loading user data for {0:d} users ...", userData.size());
    }

    std::optional<UserData*> user;
    std::vector<std::unique_ptr<Subset>> solutions;
    if (userData.size() == 0) {
        std::pair<std::unique_ptr<Subset>, size_t> solution = 
            appData.loadWhileStreaming ? 
                loadWhileCalculating(appData, std::nullopt, timers) : 
                loadThenCalculate(appData, std::nullopt, timers);
        spdlog::info("Finished streaming and found solution of size {0:d} and score {1:f}", solution.first->size(), solution.first->getScore());
        solutions.push_back(std::move(solution.first));
    } else {
        for (const auto & user : userData) {
            std::pair<std::unique_ptr<Subset>, size_t> solution = 
                appData.loadWhileStreaming ? 
                    loadWhileCalculating(appData, user.get(), timers) : 
                    loadThenCalculate(appData, user.get(), timers);
            spdlog::info("Finished streaming and found solution of size {0:d} and score {1:f}", solution.first->size(), solution.first->getScore());
            solutions.push_back(UserOutputInformationSubset::translate(std::move(solution.first), *user));
        }
    }
    
    std::vector<std::unique_ptr<DataRow>> dummyData;
    std::vector<size_t> dummyRowMapping;
    std::unordered_map<size_t, size_t> emptyMapping;
    size_t dummyColumns = 0;  // A placeholder for columns, set to 0 or any valid number.

    LoadedSegmentedData dummySegmentedData(
        std::move(dummyData), std::move(dummyRowMapping), std::move(emptyMapping), dummyColumns
    );

    nlohmann::json result = Orchestrator::buildOutput(
        appData, solutions, dummySegmentedData, timers
    );
    std::ofstream outputFile;
    outputFile.open(appData.outputFile);
    outputFile << result.dump(2);
    outputFile.close();

    return 0;
}