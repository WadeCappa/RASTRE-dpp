#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <limits.h>

#include<cstdlib>

#include <random>
#include <optional>

const static std::string EMPTY_STRING = "\n";

struct loadInput {
    std::string inputFile = EMPTY_STRING;
} typedef LoadInput;

struct generateInput {
    // should be an enum
    int generationStrategy = 0;
    size_t genRows;
    size_t genCols;
    double sparsity = -1; // default value
    std::string seed = EMPTY_STRING;
} typedef GenerateInput;

struct appData{
    std::string outputFile;
    size_t outputSetSize;
    unsigned int adjacencyListColumnCount = 0;
    bool binaryInput = false;
    bool normalizeInput = false;
    double epsilon = -1;
    unsigned int algorithm;
    unsigned int distributedAlgorithm = 2;
    double distributedEpsilon = 0.13;
    unsigned int threeSieveT;
    double alpha = 1;

    int worldSize = 1;
    int worldRank = 0;
    size_t numberOfDataRows;

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
            default:
                throw new std::invalid_argument("Could not find algorithm");
        }
    }

    static nlohmann::json buildDatasetJson(const BaseData &data, const AppData &appData) {
        nlohmann::json output {
            {"rows", data.totalRows()},
            {"columns", data.totalColumns()},
            {"inputFile", appData.loadInput.inputFile}
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
        app.add_flag("--loadBinary", appData.binaryInput, "Use this flag if you want to load a binary input file.");
        app.add_flag("--normalizeInput", appData.normalizeInput, "Use this flag to normalize each input vector.");
    
        CLI::App *loadInput = app.add_subcommand("loadInput", "loads the requested input from the provided path");
        CLI::App *genInput = app.add_subcommand("generateInput", "generates synthetic data");

        genInput->add_option("-g,--generationStrategy", appData.generateInput.generationStrategy);
        genInput->add_option("--genRows", appData.generateInput.genRows)->required();
        genInput->add_option("--genCols", appData.generateInput.genCols)->required();
        genInput->add_option("--sparsity", appData.generateInput.sparsity)->required();
        genInput->add_option("--generationSeed", appData.generateInput.seed);

        loadInput->add_option("-i,--input", appData.loadInput.inputFile, "Path to input file. Should contain data in row vector format.")->required();
    }

    static void addMpiCmdOptions(CLI::App &app, AppData &appData) {
        Orchestrator::addCmdOptions(app, appData);
        app.add_option("-n,--numberOfRows", appData.numberOfDataRows, "The number of total rows of data in your input file.")->required();
        app.add_option("-d,--distributedAlgorithm", appData.distributedAlgorithm, "0) randGreedi\n1) SieveStreaming\n2) ThreeSieves\nDefaults to ThreeSieves");
        app.add_option("--distributedEpsilon", appData.distributedEpsilon, "Only used for streaming. Defaults to 0.13.");
        app.add_option("-T,--threeSieveT", appData.threeSieveT, "Only used for ThreeSieveStreaming.");
        app.add_option("--alpha", appData.alpha, "Only used for the truncated setting.");
    }

    static std::unique_ptr<FullyLoadedData> getData(AppData& appData) {
        // build data step
        if (appData.loadInput.inputFile == EMPTY_STRING) {
            if (appData.generateInput.seed == EMPTY_STRING) {
            }

        } else {
            std::ifstream inputFile;
            inputFile.open(appData.loadInput.inputFile);
            std::unique_ptr<FullyLoadedData> data(Orchestrator::buildData(appData, inputFile));
            inputFile.close();
            return move(data);
        }
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
        const unsigned int lowestMachine = appData.distributedAlgorithm == 0 ? 0 : 1;
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
            {"epsilon", appData.epsilon},
            {"Rows", solution.toJson()},
            {"dataset", buildDatasetJson(data, appData)},
            {"worldSize", appData.worldSize}
        };

        return output;
    }

    static std::unique_ptr<SegmentedData> buildMpiData(
        const AppData& appData, 
        std::istream &data, 
        const std::vector<unsigned int> &rowToRank
    ) {
        std::unique_ptr<DataRowFactory> factory(getDataRowFactory(appData));
        return SegmentedData::load(*factory, data, rowToRank, appData.worldRank);
    }

    static std::unique_ptr<FullyLoadedData> buildData(const AppData& appData, std::istream &data) {
        std::unique_ptr<DataRowFactory> factory(getDataRowFactory(appData));
        return FullyLoadedData::load(*factory, data);
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