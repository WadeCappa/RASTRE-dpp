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
#include <fstream>

struct UserModeAppData{
    std::string outputFile;
    std::string inputFile;
    std::string userModeFile;
};

int main(int argc, char** argv) {
    LoggerHelper::setupLoggers();
    CLI::App app{"Approximates the best possible approximation set for the input dataset."};
    AppData appData;
    Orchestrator::addCmdOptions(app, appData);
    std::string userModeOutputFile;
    app.add_option("--userModeOutputFile", userModeOutputFile, "The output file from running a user-mode job")->required();
    CLI11_PARSE(app, argc, argv);
    appData.worldRank = 0;
    appData.worldSize = 1;

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

    std::map<unsigned long long, std::unique_ptr<UserData>> userMap;
    if (appData.userModeFile != EMPTY_STRING) {
        std::vector<std::unique_ptr<UserData>> userData (UserDataImplementation::load(appData.userModeFile));
        spdlog::info("Finished loading user data for {0:d} users ...", userData.size());
        for (size_t i = 0; i < userData.size(); i++) {
            spdlog::debug("user {0:d} has ru of size {1:d}", userData[i]->getUserId(), userData[i]->getRu().size());
            userMap.insert({userData[i]->getUserId(), move(userData[i])});
        }
    }

    std::ifstream file(userModeOutputFile);
    nlohmann::json userModeOutputData(nlohmann::json::parse(file));

    double total_mrr = 0.0, total_ilad = 0.0, total_ilmd = 0.0;
    size_t total_solutions = userModeOutputData["solutions"].size();
    std::unique_ptr<RelevanceCalculator> calc(NaiveRelevanceCalculator::from(*data));

    for (const auto & solution : userModeOutputData["solutions"]) {
        unsigned long long user_id = solution["userId"];
        auto userSolution = solution["solution"];
        std::vector<size_t> rows(userSolution["rows"].begin(), userSolution["rows"].end());
        std::unique_ptr<Subset> subset(Subset::of(rows, userSolution["totalCoverage"]));

        const double mrr = UserScore::calculateMRR(*userMap[user_id], *subset);

        const std::vector<double> &ru = userMap[user_id]->getRu();
        std::vector<double> scores(ru.size() * ru.size());
        #pragma omp parallel for 
        for (size_t j = 0; j < ru.size(); j++) {
            for (size_t i = 0; i < ru.size(); i++) {
                if (j == i) {
                    continue;
                }

                scores[j * ru.size() + i] = 1.0 - calc->get(i, j);
            }
        }

        double aggregate_scores = 0.0;
        double ilmd = std::numeric_limits<double>::max();
        for (const auto & s : scores) {
            aggregate_scores += s;
            ilmd = std::min(ilmd, s);
        }
        double ilad = aggregate_scores / (double)(ru.size() * 2.0);
        spdlog::info("User {0:d}: MRR = {1:f}, ILAD = {2:f}, ILMD = {3:f}", user_id, mrr, ilad, ilmd);
        total_mrr += mrr;
        total_ilad += ilad; 
        total_ilmd += ilmd;
    }

    spdlog::info(
        "RESULT: MRR = {0:f}, ILAD = {1:f}, ILMD = {2:f}", total_mrr / (double)total_solutions, total_ilad / (double)total_solutions, total_ilmd / (double)total_solutions
    );

    return 0;
}