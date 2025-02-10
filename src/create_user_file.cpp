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

struct GenUserFileAppData {
    std::string outputFile;
    std::string inputFile;
    float topUPercentage;
    unsigned int nopN;

    unsigned int adjacencyListColumnCount = 0;

    size_t numberOfRowNonZeros = 0;
    size_t numberOfColumnNonZeros = 0;
};

static void addCmdOptions(CLI::App &app, GenUserFileAppData &appData) {
    app.add_option("-o,--output", appData.outputFile, "Path to output file.")->required();
    app.add_option("-i,--input", appData.inputFile, "Path to input file.")->required();
    app.add_option("--rowNonZeros", appData.numberOfRowNonZeros, "The number of row nonzeros required for a row to be considered. All other rows are dropped")->required();
    app.add_option("--columnsNonZeros", appData.numberOfColumnNonZeros, "The number of column nonzeros required for a column to be considered. All other columns are dropped")->required();

    app.add_option("--adjacencyListColumnCount", appData.adjacencyListColumnCount, "To load an adjacnency list, set this value to the number of columns per row expected in the underlying matrix.");
}

static std::vector<size_t> getColumnsAboveThreshold(
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

    std::vector<size_t> res;
    for (size_t i = 0; i < countPerColumn.size(); i++) {
        if (countPerColumn[i] >= nonZeroThreshold) {
            res.push_back(i);
        }
    }

    return move(res);
}

static std::vector<size_t> getRowsAboveThreshold(
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

    std::vector<size_t> res;
    for (size_t i = 0; i < data.totalRows(); i++) {
        PassesRowThresholdVisitor v(nonZeroThreshold);
        const DataRow& row(data.getRow(i));
        if (row.visit(v)) {
            res.push_back(i);
        }
    }

    return res;
}

int main(int argc, char** argv) {
    LoggerHelper::setupLoggers();
    CLI::App app{"Calculates the scores for a given output, set of user data, and input file"};
    GenUserFileAppData appData;
    addCmdOptions(app, appData);
    std::string userModeOutputFile;
    CLI11_PARSE(app, argc, argv);

    std::ifstream inputFile;
    inputFile.open(appData.inputFile);
    std::unique_ptr<LineFactory> getter(
        std::unique_ptr<FromFileLineFactory>(
            new FromFileLineFactory(inputFile)
        )
    );

    std::unique_ptr<DataRowFactory> factory(Orchestrator::getDataRowFactory(appData.adjacencyListColumnCount, true));
    std::unique_ptr<BaseData> data(FullyLoadedData::load(*factory, *getter));
    inputFile.close();

    spdlog::info("Finished loading dataset of size {0:d} ...", data->totalRows());

    std::vector<size_t> rowsToEvaluate(getRowsAboveThreshold(*data, appData.numberOfRowNonZeros));

    spdlog::info("found {0:d} rows above {1:d} non-zeros", rowsToEvaluate.size(), appData.numberOfRowNonZeros);

    std::vector<size_t> columnsToEvaluate(getColumnsAboveThreshold(*data, appData.numberOfColumnNonZeros));

    spdlog::info("found {0:d} columnes above {1:d} non-zeros", columnsToEvaluate.size(), appData.numberOfColumnNonZeros);

    return 0;
}