
#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include <fstream>

#include "log_macros.h"
#include "representative_subset_calculator/orchestrator/orchestrator.h"
#include "user_mode/user_data.h"
#include "user_mode/user_score.h"

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
        std::vector<std::vector<double>> scores(ru.size(), std::vector<double>(ru.size()));
        double ilmd = std::numeric_limits<double>::max();
        double aggregate_scores = 0.0;
        for (size_t j = 0; j < ru.size(); j++) {
            for (size_t i = 0; i < ru.size(); i++) {
                if (j == i) {
                    continue;
                }

                const float v = 1.0 - calc->get(i, j); 
                SPDLOG_TRACE("i of {0:d} and j of {1:d} got score of {2:f}", i, j, v);
                scores[j][i] = v;
                ilmd = std::min(ilmd, (double)v);
                aggregate_scores += v;
                if (scores[j][i] > 1.0) {
                    spdlog::error("score of {0:f} > 1 for j {1:d} and i {2:d}", scores[j][i], j, i);
                }
            }
        }

        // we subtract the diagonals since we don't include them in our score
        const double total_number_of_pairs = (std::pow(ru.size(), 2) - (double)ru.size());
        SPDLOG_DEBUG("Going to divide {0:f} by the total number of pairs of {1:f}", aggregate_scores, total_number_of_pairs);
        double ilad = aggregate_scores / total_number_of_pairs;
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