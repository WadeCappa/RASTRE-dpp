
#include <string>

#include <nlohmann/json.hpp>

#include "app_data_constants.h"

#ifndef APP_DATA_H
#define APP_DATA_H

struct AppData{
    public:

    struct loadInput {
        std::string inputFile = NO_FILE_DEFAULT;
    } typedef LoadInput;

    struct generateInput {
        int generationStrategy = 0;
        size_t genRows = 0;
        size_t genCols = 0;
        float sparsity = DEFAULT_GENERATED_SPARSITY;
        long unsigned int seed = -1;
    } typedef GenerateInput;

    std::string outputFile;
    size_t outputSetSize;
    unsigned int adjacencyListColumnCount = 0;
    bool binaryInput = false;
    float epsilon = -1;
    unsigned int algorithm;
    unsigned int distributedAlgorithm = 2;
    float distributedEpsilon = 0.13;
    unsigned int threeSieveT;
    float alpha = 1;
    bool stopEarly = false;
    bool loadWhileStreaming = false;
    bool sendAllToReceiver = false;
    bool doNotNormalizeOnLoad = false;
    
    // user mode config
    std::string userModeFile = NO_FILE_DEFAULT;
    double theta = 0.7; // defaults to 70% focus on relevance, 30% focus on diversity
    int worldSize = 1;
    int worldRank = 0;
    size_t numberOfDataRows = 0;
    LoadInput loadInput;
    GenerateInput generateInput;

    static nlohmann::json toJson(const AppData& appData) {
        nlohmann::json output {
            {"k", appData.outputSetSize}, 
            {"algorithm", algorithmToString(appData)},
            {"inputSettings", getInputSettings(appData)},
            {"epsilon", appData.epsilon},
            {"worldSize", appData.worldSize}
        };
        return output;
    }
    
    private: 
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
};

#endif