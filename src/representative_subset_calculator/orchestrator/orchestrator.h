#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <limits.h>

#include<cstdlib>

#include <random>
#include <optional>

const static std::string EMPTY_STRING = "\n";
const static float DEFAULT_GENERATED_SPARSITY = -1;

struct loadInput {
    std::string inputFile = EMPTY_STRING;
    unsigned int multiFile = 0;
    std::string directory = EMPTY_STRING;
} typedef LoadInput;

struct generateInput {
    // should be an enum
    int generationStrategy = 0;
    size_t genRows = 0;
    size_t genCols = 0;
    float sparsity = DEFAULT_GENERATED_SPARSITY;
    long unsigned int seed = -1;
} typedef GenerateInput;

struct appData{
    std::string outputFile;
    size_t outputSetSize;
    unsigned int adjacencyListColumnCount = 0;
    bool binaryInput = false;
    bool normalizeInput = false;
    float epsilon = -1;
    unsigned int algorithm;
    unsigned int distributedAlgorithm = 2;
    float distributedEpsilon = 0.13;
    unsigned int threeSieveT;
    float alpha = 1;
    bool stopEarly = false;
    bool loadWhileStreaming = false;

    int worldSize = 1;
    int worldRank = 0;
    size_t numberOfDataRows = 0;

   LoadInput loadInput;
   GenerateInput generateInput;
} typedef AppData;

class Orchestrator {
    public:
    static std::string algorithmToString(const AppData &appData) {
        switch (appData.algorithm) {
            case 0:
                return "naive greedy";
            case 1:
                return "lazy greedy";
            case 2:
                return "fast greedy";
            case 3:
                return "lazy fast greedy";
            case 4:
                return "streaming";
            default:
                throw new std::invalid_argument("Could not find algorithm");
        }
    }

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

    static nlohmann::json getInputSettings(const AppData &appData) {
        nlohmann::json output {
            {"inputFile", appData.loadInput.inputFile},
            {"generationStrategy", appData.generateInput.generationStrategy}, 
            {"generatedRows", appData.generateInput.genRows},
            {"generatedCols", appData.generateInput.genCols},
            {"seed", appData.generateInput.seed},
            {"sparsity", appData.generateInput.sparsity}
        };

        return output;
    }

    static nlohmann::json buildOutput(
        const AppData &appData, 
        const Subset &solution,
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
        app.add_flag("--normalizeInput", appData.normalizeInput, "Use this flag to normalize each input vector.");
        app.add_flag("--stopEarly", appData.stopEarly, "Used excusevly during streaming to stop the execution of the program early. If you use this in conjuntion with randgreedi, you will lose your approximation guarantee");
    
        CLI::App *loadInput = app.add_subcommand("loadInput", "loads the requested input from the provided path");
        CLI::App *genInput = app.add_subcommand("generateInput", "generates synthetic data");

        genInput->add_option("-g,--generationStrategy", appData.generateInput.generationStrategy, "Refers to the value of edges. 0)(DEFAULT) normal distribution, 1) edges are alwayas 1");
        genInput->add_option("--rows", appData.generateInput.genRows)->required();
        genInput->add_option("--cols", appData.generateInput.genCols)->required();
        genInput->add_option("--sparsity", appData.generateInput.sparsity, "Note that edges are generated uniformly at random.");
        genInput->add_option("--seed", appData.generateInput.seed)->required();

        loadInput->add_option("--multiFile", appData.loadInput.multiFile, "Load input from these many files.")->required();
        loadInput->add_option("--directory", appData.loadInput.directory, "Path to directory containing multiple files. 1 to set this 0 otherwise.");
        loadInput->add_option("-i,--input", appData.loadInput.inputFile, "Path to input file. Or pattern contained in filename.")->required();
    }

    static void addMpiCmdOptions(CLI::App &app, AppData &appData) {
        Orchestrator::addCmdOptions(app, appData);
        app.add_option("-d,--distributedAlgorithm", appData.distributedAlgorithm, "0) randGreedi\n1) SieveStreaming\n2) ThreeSieves\nDefaults to ThreeSieves\n3)Comparison Mode");
        app.add_option("--distributedEpsilon", appData.distributedEpsilon, "Only used for streaming. Defaults to 0.13.");
        app.add_option("-T,--threeSieveT", appData.threeSieveT, "Only used for ThreeSieveStreaming.");
        app.add_option("--alpha", appData.alpha, "Only used for the truncated setting.");
    }

    static SubsetCalculator* getCalculator(const AppData &appData) {
        switch (appData.algorithm) {
            case 0:
                return dynamic_cast<SubsetCalculator*>(new NaiveSubsetCalculator());
            case 1:
                return dynamic_cast<SubsetCalculator*>(new LazySubsetCalculator());
            case 2:
                return dynamic_cast<SubsetCalculator*>(new FastSubsetCalculator(appData.epsilon));
            case 3: 
                return dynamic_cast<SubsetCalculator*>(new LazyFastSubsetCalculator(appData.epsilon));
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
        const Subset &solution,
        const BaseData &data,
        const Timers &timers
    ) { 
        nlohmann::json output {
            {"k", appData.outputSetSize}, 
            {"algorithm", algorithmToString(appData)},
            {"inputSettings", getInputSettings(appData)},
            {"epsilon", appData.epsilon},
            {"Rows", solution.toJson()},
            {"worldSize", appData.worldSize}
        };

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
                    move(rng),
                    move(sparsityRng)
            );
        } else {
            return GeneratedDenseLineFactory::create(
                appData.generateInput.genRows,
                appData.generateInput.genCols,
                move(rng)
            );
        }
    }

    static std::unique_ptr<SegmentedData> buildMpiData(
        const AppData& appData, 
        GeneratedLineFactory &getter,
        const std::vector<unsigned int> &rowToRank
    ) {
        std::unique_ptr<DataRowFactory> factory(getDataRowFactory(appData));
        return SegmentedData::loadInParallel(*factory, getter, rowToRank, appData.worldRank);
    }

    static std::unique_ptr<SegmentedData> buildMpiData(
        const AppData& appData, 
        LineFactory &getter,
        const std::vector<unsigned int> &rowToRank
    ) {
        std::unique_ptr<DataRowFactory> factory(getDataRowFactory(appData));
        return SegmentedData::load(*factory, getter, rowToRank, appData.worldRank);
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
        DataRowFactory *factory;
        
        if (appData.adjacencyListColumnCount > 0) {
            factory = dynamic_cast<DataRowFactory*>(new SparseDataRowFactory(appData.adjacencyListColumnCount));
        } else {
            factory = dynamic_cast<DataRowFactory*>(new DenseDataRowFactory());
        }

        return std::unique_ptr<DataRowFactory>(factory);
    }
};