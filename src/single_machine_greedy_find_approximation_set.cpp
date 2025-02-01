#include "log_macros.h"

#include "user_mode/user_data.h"
#include "representative_subset_calculator/streaming/communication_constants.h"
#include "representative_subset_calculator/representative_subset.h"
#include "data_tools/data_row_visitor.h"
#include "data_tools/to_binary_visitor.h"
#include "data_tools/dot_product_visitor.h"
#include "data_tools/data_row.h"
#include "data_tools/data_row_factory.h"
#include "data_tools/base_data.h"
#include "representative_subset_calculator/timers/timers.h"
#include "data_tools/user_mode_data.h"
#include "representative_subset_calculator/kernel_matrix/relevance_calculator.h"
#include "representative_subset_calculator/kernel_matrix/relevance_calculator_factory.h"
#include "representative_subset_calculator/kernel_matrix/kernel_matrix.h"
#include "representative_subset_calculator/naive_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_representative_subset_calculator.h"
#include "representative_subset_calculator/fast_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_fast_representative_subset_calculator.h"
#include "representative_subset_calculator/orchestrator/orchestrator.h"
#include "representative_subset_calculator/memoryProfiler/MemUsage.h"
#include "user_mode/user_score.h"
#include "user_mode/user_subset.h"

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

int main(int argc, char** argv) {
    LoggerHelper::setupLoggers();
    CLI::App app{"Approximates the best possible approximation set for the input dataset."};
    AppData appData;
    Orchestrator::addCmdOptions(app, appData);
    CLI11_PARSE(app, argc, argv);
    appData.worldRank = 0;
    appData.worldSize = 1;

    Timers timers;

    spdlog::info("Starting standalone greedy...");
    
    auto baseline = getPeakRSS();
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

    std::unique_ptr<BaseData> data(Orchestrator::loadData(appData, *getter.get()));
    if (appData.loadInput.inputFile != EMPTY_STRING) {
        inputFile.close();
    } 

    spdlog::info("Finished loading dataset of size {0:d} ...", data->totalRows());

    std::vector<std::unique_ptr<UserData>> userData;
    if (appData.userModeFile != EMPTY_STRING) {
        userData = UserDataImplementation::load(appData.userModeFile);
        spdlog::info("Finished loading user data for {0:d} users ...", userData.size());
    }

    timers.loadingDatasetTime.stopTimer();

    timers.totalCalculationTime.startTimer();

    std::vector<std::unique_ptr<Subset>> solutions;

    std::unique_ptr<SubsetCalculator> calculator(Orchestrator::getCalculator(appData));
    if (userData.size() == 0) {
        NaiveRelevanceCalculator calc(*data);
        solutions.push_back(calculator->getApproximationSet(NaiveMutableSubset::makeNew(), calc, *data, appData.outputSetSize));
        spdlog::info("Found solution of size {0:d} and score {1:f}", solutions.back()->size(), solutions.back()->getScore());
    } else {
        for (const auto & user : userData) {
            std::unique_ptr<UserModeDataDecorator> decorator(
                UserModeDataDecorator::create(*data, *user)
            );
            std::unique_ptr<RelevanceCalculator> userCalc(UserModeRelevanceCalculator::from(*decorator, *user, appData.theta));
            std::unique_ptr<Subset> solution(calculator->getApproximationSet(
                NaiveMutableSubset::makeNew(), *userCalc, *decorator, appData.outputSetSize)
            );
            solutions.push_back(UserOutputInformationSubset::translate(move(solution), *user));
            spdlog::info("Found solution of size {0:d} and score {1:f}", solutions.back()->size(), solutions.back()->getScore());
        }
    }

    timers.totalCalculationTime.stopTimer();
    auto memUsage = getPeakRSS()- baseline;
    spdlog::info("mem usage of %d", memUsage);
    nlohmann::json result = Orchestrator::buildOutput(appData, solutions, *data.get(), timers);
    std::ofstream outputFile;
    outputFile.open(appData.outputFile);
    outputFile << result.dump(2);
    outputFile.close();

    return 0;
}