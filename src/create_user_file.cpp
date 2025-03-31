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
#include "representative_subset_calculator/fast_representative_subset_calculator.h"
#include "representative_subset_calculator/lazy_fast_representative_subset_calculator.h"
#include "representative_subset_calculator/orchestrator/orchestrator.h"
#include "representative_subset_calculator/memoryProfiler/MemUsage.h"
#include "user_mode/user_score.h"
#include "user_mode/user_subset.h"
#include <fstream>
#include <sstream> 

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <fstream>

const static std::string NO_FILE_SPECIFIED = "no-file-specified";

struct GenUserFileAppData {
    std::string outputFile;
    std::string inputFile;
    unsigned int topN;

    unsigned int adjacencyListColumnCount = 0;

    size_t numberOfRowNonZeros = 0;
    size_t numberOfColumnNonZeros = 0;

    std::string userListFile = NO_FILE_SPECIFIED;
    float topU;
};

static void addCmdOptions(CLI::App &app, GenUserFileAppData &appData) {
    app.add_option("-o,--output", appData.outputFile, "Path to output file to write usermode data.")->required();
    app.add_option("-i,--input", appData.inputFile, "Path to input dataset. This is the file that we'll be generating usermode date for.")->required();

    app.add_option("--adjacencyListColumnCount", appData.adjacencyListColumnCount, "To load an adjacnency list, set this value to the number of columns per row expected in the underlying matrix.");

    app.add_option("--rowNonZeros", appData.numberOfRowNonZeros, "The number of row nonzeros required for a row to be considered. All other rows are dropped")->required();
    app.add_option("--columnsNonZeros", appData.numberOfColumnNonZeros, "The number of column nonzeros required for a column to be considered. All other columns are dropped")->required();
    app.add_option("--topN", appData.topN, "The number of elements to consider for CU per element in PU")->required();

    CLI::App *usersFromFileCommand = app.add_subcommand("usersFromFile", "User this command if you have a set of users you want to generate usermode data for.");
    CLI::App *topUsersCommand = app.add_subcommand("topUsers", "Use this command if you want to generate usermode data for the top users in the dataset.");

    usersFromFileCommand->add_option("--userListFile", appData.userListFile, "This file should be an integer list delimited by ' '. Each integer is a user that we'll try to load from the specified dataset in --input.")->required();
    topUsersCommand->add_option("--topU", appData.topU, "The ratio of users from the underlying dataset that should be evaluated. For example, set to 1.0 to evaluate all users, 0.05 to evaluate 5%.")->required();
}

static std::unordered_set<size_t> getColumnsAboveThreshold(
    const BaseData& data, const size_t nonZeroThreshold
) {
    class PassesColumnThresholdVisitor : public DataRowVisitor {
        private:
        std::vector<size_t>& countPerColumn;

        public:
        PassesColumnThresholdVisitor(std::vector<size_t>& countPerColumn)
        : countPerColumn(countPerColumn) {}

        std::vector<size_t> get() {
            return move(countPerColumn);
        }

        void visitDenseDataRow(const std::vector<float>& data) {
            for (size_t i = 0; i < data.size(); i++) {
                countPerColumn[i] += static_cast<bool>(data[i] != 0);
            }
        }

        void visitSparseDataRow(const std::map<size_t, float>& data, size_t _totalColumns) {
            for (const auto & p : data) {
                countPerColumn[p.first]++;
            }
        }
    };

    std::vector<size_t> countPerColumn(data.totalColumns());
    PassesColumnThresholdVisitor v(countPerColumn);
    for (size_t r = 0; r < data.totalRows(); r++) {
        const DataRow& row(data.getRow(r));
        row.voidVisit(v);
    }

    std::unordered_set<size_t> res;
    for (size_t i = 0; i < countPerColumn.size(); i++) {
        if (countPerColumn[i] >= nonZeroThreshold) {
            res.insert(i);
        }
    }

    return move(res);
}

static std::unordered_set<size_t> getRowsAboveThreshold(
    const BaseData& data, const size_t nonZeroThreshold
) {
    class PassesRowThresholdVisitor : public ReturningDataRowVisitor<bool> {
        private:
        const size_t nonZeroThreshold;
        bool success;

        public:
        PassesRowThresholdVisitor(const size_t nonZeroThreshold) 
        : nonZeroThreshold(nonZeroThreshold), success(false) {}

        bool get() {
            return success;
        }

        void visitDenseDataRow(const std::vector<float>& data) {
            size_t c = 0;
            for (const float d : data) {
                c += static_cast<bool>(d != 0.0);
            }
            success = c >= nonZeroThreshold;
        }

        void visitSparseDataRow(const std::map<size_t, float>& data, size_t _totalColumns) {
            success = data.size() >= nonZeroThreshold;
        }
    };

    std::unordered_set<size_t> res;
    for (size_t i = 0; i < data.totalRows(); i++) {
        PassesRowThresholdVisitor v(nonZeroThreshold);
        const DataRow& row(data.getRow(i));
        if (row.visit(v)) {
            res.insert(i);
        }
    }

    return res;
}

std::vector<size_t> rowsThatReferenceUser(
    size_t user, const std::unordered_set<size_t>& rowsToEvaluate, const BaseData& data
) {
    class RowReferencesUser : public ReturningDataRowVisitor<bool> {
        private:
        bool success;
        const size_t user;

        public:
        RowReferencesUser(const size_t user) : user(user), success(false) {}

        bool get() {
            return success;
        }

        void visitDenseDataRow(const std::vector<float>& data) {
            success = data[user] != 0;
        }

        void visitSparseDataRow(const std::map<size_t, float>& data, size_t _totalColumns) {
            success = data.find(user) != data.end();
        }
    };

    std::vector<size_t> res;
    RowReferencesUser v(user);
    for (const size_t r : rowsToEvaluate) {
        if (data.getRow(r).visit(v)) {
            res.push_back(r);
        }
    }

    return move(res);
}

std::unordered_set<size_t> loadUsersFromFile(
    const GenUserFileAppData& appData, 
    const FullyLoadedData& data, 
    const std::unordered_set<size_t>& columnsToEvaluate) {

    std::unordered_set<size_t> result;
    std::ifstream input;
    input.open(appData.userListFile);
    std::string row;
    while (std::getline(input, row)) {
        std::istringstream iss(row);
        std::string num;
        while (std::getline(iss, num, ' ')) {
            std::stringstream sstream(num);
            size_t user;
            sstream >> user;
            if (columnsToEvaluate.find(user) == columnsToEvaluate.end()) {
                spdlog::warn("User {0:d} could not be found in your dataset. Try lowering numberOfColumnNonZeros. Skipping user", user);
                continue;
            }
            result.insert(user);
        }
    }
    input.close();
    return move(result);
}

std::unordered_set<size_t> selectTopUsers(
    const GenUserFileAppData& appData, 
    const FullyLoadedData& data, 
    const std::vector<size_t>& columnsToEvaluate) {

    // randomly sample topU % users
    std::unordered_set<size_t> topUsers;

    std::random_device rd; 
    std::mt19937 eng(rd());
    std::uniform_int_distribution<> uniformDistribution(0, columnsToEvaluate.size() - 1);

    size_t totalUsersToEvaluate = std::floor((double)data.totalColumns() * (double)appData.topU);
    for (size_t i = 0; i < totalUsersToEvaluate; i++) {
        topUsers.insert(columnsToEvaluate[uniformDistribution(eng)]);
    }

    return move(topUsers);
}

int main(int argc, char** argv) {
    LoggerHelper::setupLoggers();
    CLI::App app{"Calculates the scores for a given output, set of user data, and input file"};
    GenUserFileAppData appData;
    addCmdOptions(app, appData);
    std::string userModeOutputFile;
    CLI11_PARSE(app, argc, argv);

    if (appData.userListFile == NO_FILE_SPECIFIED && appData.topU > 1.0) {
        throw std::invalid_argument("topU cannot be greater than 1.0");
    }

    std::ifstream inputFile;
    inputFile.open(appData.inputFile);
    std::unique_ptr<LineFactory> getter(
        std::unique_ptr<FromFileLineFactory>(
            new FromFileLineFactory(inputFile)
        )
    );

    std::unique_ptr<DataRowFactory> factory(Orchestrator::getDataRowFactory(appData.adjacencyListColumnCount, true));
    std::unique_ptr<FullyLoadedData> data(FullyLoadedData::load(*factory, *getter));
    inputFile.close();

    spdlog::info("Finished loading dataset of rows {0:d} and columns {1:d} ...", data->totalRows(), data->totalColumns());

    std::unordered_set<size_t> rowsToEvaluate(getRowsAboveThreshold(*data, appData.numberOfRowNonZeros));

    spdlog::info("found {0:d} rows above {1:d} non-zeros", rowsToEvaluate.size(), appData.numberOfRowNonZeros);

    std::unordered_set<size_t> columnsToEvaluate(getColumnsAboveThreshold(*data, appData.numberOfColumnNonZeros));

    spdlog::info("found {0:d} columnes above {1:d} non-zeros", columnsToEvaluate.size(), appData.numberOfColumnNonZeros);

    size_t allColumns = data->totalColumns();
    std::unordered_set<size_t> removeSet;
    for (size_t c = 0; c < allColumns; c++) {
        if (columnsToEvaluate.find(c) == columnsToEvaluate.end()) {
            removeSet.insert(c);
        }
    }

    data = data->withoutColumns(removeSet);

    spdlog::info("dropped {0:d} columns from the underlying dataset", removeSet.size());

    std::vector<size_t> v(columnsToEvaluate.begin(), columnsToEvaluate.end());
    std::unordered_set<size_t> usersToEvaluate(appData.userListFile == NO_FILE_SPECIFIED ? selectTopUsers(appData, *data, v) : loadUsersFromFile(appData, *data, columnsToEvaluate));

    spdlog::info("evalulating {0:d} users", usersToEvaluate.size());

    // for each user 
        // build PU from the rows that reference that user
        // select a random list item that acts as our test row. We select this out of the total rows for that user
    std::unordered_map<size_t, std::vector<size_t>> p;
    std::unordered_map<size_t, size_t> p_test;
    for (const size_t u : usersToEvaluate) {
        p.insert({u, std::vector<size_t>()});
        p_test.insert({u, 0});
    }
    for (const size_t u : usersToEvaluate) {
        std::vector<size_t> pu(rowsThatReferenceUser(u, rowsToEvaluate, *data));

        std::random_device rd; 
        std::mt19937 eng(rd());
        std::uniform_int_distribution<> uniformDistribution(0, pu.size() - 1);
        const size_t i = uniformDistribution(eng);
        const size_t pu_test = pu[i];

        spdlog::info("pu for user {0:d} is of size {1:d} and has a test id of {2:d}", u, pu.size(), pu_test);

        pu.erase(pu.begin() + i);
        p[u] = move(pu);
        p_test[u] = pu_test;
    }

    // for each user we build CU by
        // for each row in PU
            // look at the similarities of that row to all other rows.
            // select topN rows with highest similarty, excluding the self comparison 
            // CU = CU.union(set from step above)
    std::unordered_map<size_t, std::unordered_set<size_t>> c;
    for (const size_t u : usersToEvaluate) {
        c.insert({u, std::unordered_set<size_t>()});
    }

    std::vector<size_t> userList(usersToEvaluate.begin(), usersToEvaluate.end());

    std::unique_ptr<NaiveRelevanceCalculator> calc(NaiveRelevanceCalculator::from(*data));
    std::unique_ptr<LazyKernelMatrix> matrix(ThreadSafeLazyKernelMatrix::from(*data, *calc));

    #pragma omp parallel for
    for (size_t k = 0; k < userList.size(); k++) {

        const size_t u = userList[k];

        const std::vector<size_t>& pu = p[u];
        const std::unordered_set<size_t> elements_in_pu(pu.begin(), pu.end());
        std::unordered_set<size_t> cu;

        for (const size_t i : pu) {
            std::vector<std::pair<size_t, double>> scores;
            for (const size_t j : rowsToEvaluate) {
                if (i == j || elements_in_pu.find(j) != elements_in_pu.end()) {
                    continue;
                }

                const double score = matrix->get(i, j);
                scores.push_back({j, score});
            }

            auto comp = [](auto &left, auto &right) {
                return left.second < right.second;
            };
            std::make_heap(scores.begin(), scores.end(), comp);
            const size_t elementsToPop = appData.topN >= scores.size() ? scores.size() : appData.topN;
            for (size_t i = 0; i < elementsToPop; i++) {
                cu.insert(scores.front().first);
                std::pop_heap(scores.begin(), scores.end(), comp);
                scores.pop_back();
            }
        }

        spdlog::info("cu for user {0:d} is of size {1:d}", u, cu.size());
        c[u] = move(cu);
    }

    // RU also requires comparisons. See python script.
    std::unordered_map<size_t, std::unordered_map<size_t, double>> r;
    for (const size_t u : usersToEvaluate) {
        r.insert({u, std::unordered_map<size_t, double>()});
    }
    #pragma omp parallel for
    for (size_t k = 0; k < userList.size(); k++) {
        const size_t u = userList[k];
        std::unordered_map<size_t, double> ru;
        double magnitude = 0.0;
        for (const size_t cu_i : c[u]) {
            double score = 0.0;
            for (const size_t pu_j : p[u]) {
                score += matrix->get(cu_i, pu_j);
            }
            ru.insert({cu_i, score});
            magnitude += std::pow(score, 2);
        }

        magnitude = std::sqrt(magnitude);
        spdlog::info("ru for user {0:d} is of size {1:d} and magnitude {2:f}", u, ru.size(), magnitude);
        for (const size_t cu_i : c[u]) {
            ru[cu_i] = ru.at(cu_i) / magnitude;
        }
        r[u] = move(ru);
    }

    // Order of information per row
    // UID TID LCU CU1 CU2 ... CU_LCU RU1 ... RU_LRU
    // UID: User ID
    // TID: Test item excluded from PU for that user
    // LCU: Length of CU
    // CU1 ... CU_LCU: Items in CU (total LCU items)
    // RU1 ... RU_LRU: Items in RU which is also of size LCU

    std::ofstream out;
    out.open(appData.outputFile);
    for (const size_t u : usersToEvaluate) {
        out << u << " " << p_test.at(u) << " " << r.at(u).size() << " ";

        std::vector<double> ruOrder;
        for (const auto& cuAndRu : r.at(u)) {
            out << cuAndRu.first << " ";
            ruOrder.push_back(cuAndRu.second);
        }
        for (const double ru : ruOrder) {
            out << ru << " ";
        }
        out << std::endl;
        spdlog::info("wrote user {0:d} with t_id {1:d}, l_pu {2:d}, and l_cu {3:d}", u, p_test.at(u), p.at(u).size(), r.at(u).size());
    }
    out.close();

    return 0;
}