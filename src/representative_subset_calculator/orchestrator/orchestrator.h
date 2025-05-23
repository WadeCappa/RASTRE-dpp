#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <limits.h>
#include<cstdlib>
#include <random>
#include <optional>

#include "../fast_representative_subset_calculator.h"
#include "../lazy_fast_representative_subset_calculator.h"
#include "../timers/timers.h"
#include "app_data.h"

#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

class Orchestrator {
    public:
    static nlohmann::json buildDatasetJson(const BaseData &data, const AppData &appData) {
        Diagnostics diagnostics = data.DEBUG_getDiagnostics();
        nlohmann::json output {
            {"rows", data.totalRows()},
            {"columns", data.totalColumns()},
            {"sparsity", diagnostics.sparsity},
            {"nonEmptyCells", diagnostics.numberOfNonEmptyCells}
        };

        return output;
    }

    static nlohmann::json buildOutput(
        const AppData &appData, 
        const std::vector<std::unique_ptr<Subset>> &solution,
        const BaseData &data,
        const Timers &timers
    ) {
        nlohmann::json output = buildOutputBase(appData, solution, data, timers);
        output.push_back({"timings", timers.outputToJson()});
        return output;
    }

    static void addCmdOptions(CLI::App &app, AppData &appData) {
        app.add_option("-o,--output", appData.outputFile, "Path to output file.")->required();
        app.add_option("-k,--outputSetSize", appData.outputSetSize, "Sets the desired size of the representative set.")->required();
        app.add_option("-e,--epsilon", appData.epsilon, "Only used for the fast greedy variants. Determines the threshold for when seed selection is terminated.");
        app.add_option("-a,--algorithm", appData.algorithm, "Determines the seed selection algorithm. 0) naive, 1) lazy, 2) fast greedy, 3) lazy fast greedy");
        app.add_option("--adjacencyListColumnCount", appData.adjacencyListColumnCount, "To load an adjacnency list, set this value to the number of columns per row expected in the underlying matrix.");
        app.add_option("-n,--numberOfRows", appData.numberOfDataRows, "The number of total rows of data in your input file. This is needed to distribute work and is required for multi-machine mode");
        app.add_flag("--loadBinary", appData.binaryInput, "Use this flag if you want to load a binary input file.");
        app.add_flag("--stopEarly", appData.stopEarly, "Used excusevly during streaming to stop the execution of the program early. If you use this in conjuntion with randgreedi, you will lose your approximation guarantee");
        app.add_option("-u,--userModeFile", appData.userModeFile, "Path to user mode data. Only set this if you are processing a dataset for a set of users.");
        app.add_option("--userModeTheta", appData.theta, "Only used during user mode. Sets the ratio of relevance to diveristy, where a value of 0.7 is a 70\% focuse on relevance.");
        app.add_flag("--doNotNormalizeOnLoad", appData.doNotNormalizeOnLoad, "Normalize on load");
    
        CLI::App *loadInput = app.add_subcommand("loadInput", "loads the requested input from the provided path");
        CLI::App *genInput = app.add_subcommand("generateInput", "generates synthetic data");

        genInput->add_option("-g,--generationStrategy", appData.generateInput.generationStrategy, "Refers to the value of edges. 0)(DEFAULT) normal distribution, 1) edges are alwayas 1");
        genInput->add_option("--rows", appData.generateInput.genRows)->required();
        genInput->add_option("--cols", appData.generateInput.genCols)->required();
        genInput->add_option("--sparsity", appData.generateInput.sparsity, "Note that edges are generated uniformly at random.");
        genInput->add_option("--seed", appData.generateInput.seed)->required();

        loadInput->add_option("-i,--input", appData.loadInput.inputFile, "Path to input file. Should contain data in row vector format.")->required();
    }

    static void addMpiCmdOptions(CLI::App &app, AppData &appData) {
        Orchestrator::addCmdOptions(app, appData);
        app.add_option("-d,--distributedAlgorithm", appData.distributedAlgorithm, "0) randGreedi\n1) SieveStreaming\n2) ThreeSieves\nDefaults to ThreeSieves\n3)Comparison Mode");
        app.add_option("--distributedEpsilon", appData.distributedEpsilon, "Only used for streaming. Defaults to 0.13.");
        app.add_option("-T,--threeSieveT", appData.threeSieveT, "Only used for ThreeSieveStreaming.");
        app.add_option("--alpha", appData.alpha, "Only used for the truncated setting.");
        app.add_flag("--sendAllToReceiver", appData.sendAllToReceiver, "Enable this flag to skip the greedy calculation on the local nodes and to send all seeds directly to the receiver.");
        app.add_flag("--loadWhileStreaming", appData.loadWhileStreaming, "Only used during standalone streaming (or in conjunction with sendAllToReceiver). Only set this to true if your input dataset has already been randomized");
    }

    static std::unique_ptr<SubsetCalculator> getCalculator(const AppData &appData) {
        if (appData.sendAllToReceiver) {
            spdlog::warn("rank {0:d} is going to send all seeds to receiver", appData.worldRank);
            return std::unique_ptr<SubsetCalculator>(new AddAllToSubsetCalculator());
        } 

        switch (appData.algorithm) {
            case 0:
                throw std::invalid_argument("The naive subset calculator is no longer supported and may not perform as expected. Use algorithm 3 or 2.");
            case 1:
                throw std::invalid_argument("The naive subset calculator is no longer supported and may not perform as expected. Use algorithm 3 or 2.");
            case 2:
                return std::unique_ptr<SubsetCalculator>(new FastSubsetCalculator(appData.epsilon));
            case 3: 
                return std::unique_ptr<SubsetCalculator>(new LazyFastSubsetCalculator(appData.epsilon));
            default:
                throw new std::invalid_argument("Could not find algorithm");
        }
    }

    // TODO: This can be improved. Instead of returning a vector that is the same size of all rows that includes all rank data, return only
    //  the rows that this given rank cares about. This will improve performance while loading the dataset.
    static std::vector<unsigned int> getRowToRank(const AppData &appData, const int seed) {
        std::vector<unsigned int> rowToRank(appData.numberOfDataRows, -1);
        // Rank 0 won't have local solvers irrespective of aggregation strategy
        const unsigned int lowestMachine = 1;
        std::uniform_int_distribution<int> uniform_distribution(lowestMachine, appData.worldSize - 1);
        std::default_random_engine number_selecter(seed);

        for (int i = 0; i < appData.numberOfDataRows; i++) {
            rowToRank[i] = uniform_distribution(number_selecter);
        }

        return rowToRank;
    }

    static nlohmann::json buildOutputBase(
        const AppData &appData, 
        const std::vector<std::unique_ptr<Subset>> &solution,
        const BaseData &data,
        const Timers &timers
    ) { 
        nlohmann::json output(AppData::toJson(appData));
        output.update(nlohmann::json {
            {"solutions", outputSubsetsForSolution(solution)}
        });

        return output;
    }

    static nlohmann::json outputSubsetsForSolution(const std::vector<std::unique_ptr<Subset>> &solution) {
        nlohmann::json output;

        for (const auto & subset : solution) {
            output.push_back(subset->toJson());
        }

        return output;
    }

    static std::unique_ptr<RandomNumberGenerator> getRandomNumberGeneratorForEdges(const AppData& appData) {
        if (appData.generateInput.generationStrategy == 0) {
            return NormalRandomNumberGenerator::create(appData.generateInput.seed);
        } else if (appData.generateInput.generationStrategy == 1) {
            if (appData.generateInput.sparsity == DEFAULT_GENERATED_SPARSITY) {
                throw std::invalid_argument("Cannot use generationStrategy of 1 with a dense generator");
            }

            return AlwaysOneGenerator::create();
        }

        throw std::invalid_argument("Unrecognized generationStrategy");
    }

    static std::unique_ptr<GeneratedLineFactory> getLineGenerator(const AppData& appData) {
        std::unique_ptr<RandomNumberGenerator> rng(getRandomNumberGeneratorForEdges(appData));

        if (appData.generateInput.sparsity != DEFAULT_GENERATED_SPARSITY) {
            std::unique_ptr<RandomNumberGenerator> sparsityRng(UniformRandomNumberGenerator::create(appData.generateInput.seed + 1));
            return GeneratedSparseLineFactory::create(
                    appData.generateInput.genRows,
                    appData.generateInput.genCols,
                    appData.generateInput.sparsity,
                    std::move(rng),
                    std::move(sparsityRng)
            );
        } else {
            return GeneratedDenseLineFactory::create(
                appData.generateInput.genRows,
                appData.generateInput.genCols,
                std::move(rng)
            );
        }
    }

    static std::unique_ptr<BaseData> buildMpiData(
        const AppData& appData, 
        GeneratedLineFactory &getter,
        const std::vector<unsigned int> &rowToRank
    ) {
        std::unique_ptr<DataRowFactory> factory(getDataRowFactory(appData));
        return LoadedSegmentedData::loadInParallel(*factory, getter, rowToRank, appData.worldRank);
    }

    static std::unique_ptr<BaseData> buildMpiData(
        const AppData& appData, 
        LineFactory &getter,
        const std::vector<unsigned int> &rowToRank
    ) {
        std::unique_ptr<DataRowFactory> factory(getDataRowFactory(appData));
        return LoadedSegmentedData::load(*factory, getter, rowToRank, appData.worldRank);
    }

    static std::unique_ptr<BaseData> loadData(const AppData& appData, GeneratedLineFactory &getter) {
        std::vector<unsigned int> rowToRank(appData.numberOfDataRows, 0);
        return buildMpiData(appData, getter, rowToRank);
    }

    static std::unique_ptr<BaseData> loadData(const AppData& appData, LineFactory &getter) {
        if (appData.numberOfDataRows > 0) {
            std::vector<unsigned int> rowToRank(appData.numberOfDataRows, 0);
            return buildMpiData(appData, getter, rowToRank);
        }

        std::unique_ptr<DataRowFactory> factory(getDataRowFactory(appData));
        return FullyLoadedData::load(*factory, getter);
    }

    static std::unique_ptr<DataRowFactory> getDataRowFactory(const AppData& appData) {
        return getDataRowFactory(appData.adjacencyListColumnCount, !appData.doNotNormalizeOnLoad);
    }

    static std::unique_ptr<DataRowFactory> getDataRowFactory(
        const unsigned int columnCount,
        bool normalizeOnLoad) {
        DataRowFactory *factory;
        
        if (columnCount > 0) {
            factory = dynamic_cast<DataRowFactory*>(new SparseDataRowFactory(columnCount));
        } else {
            factory = dynamic_cast<DataRowFactory*>(new DenseDataRowFactory());
        }

        if (normalizeOnLoad) {
            factory = new NormalizedDataRowFactory(
                std::unique_ptr<DataRowFactory>(factory)
            );
        }

        return std::unique_ptr<DataRowFactory>(factory);
    }
};

#endif