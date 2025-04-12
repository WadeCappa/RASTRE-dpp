
#include <string>

#include "app_data_constants.h"

#ifndef APP_DATA_H
#define APP_DATA_H

struct appData{
    public:

    struct loadInput {
        const std::string inputFile = NO_FILE_DEFAULT;
    } typedef LoadInput;

    struct generateInput {
        const int generationStrategy = 0;
        const size_t genRows = 0;
        const size_t genCols = 0;
        const float sparsity = DEFAULT_GENERATED_SPARSITY;
        const long unsigned int seed = -1;
    } typedef GenerateInput;

    const std::string outputFile;
    const size_t outputSetSize;
    const unsigned int adjacencyListColumnCount = 0;
    const bool binaryInput = false;
    const float epsilon = -1;
    const unsigned int algorithm;
    const unsigned int distributedAlgorithm = 2;
    const float distributedEpsilon = 0.13;
    const unsigned int threeSieveT;
    const float alpha = 1;
    const bool stopEarly = false;
    const bool loadWhileStreaming = false;
    const bool sendAllToReceiver = false;
    const bool doNotNormalizeOnLoad = false;
    
    // user mode config
    const std::string userModeFile = NO_FILE_DEFAULT;
    const double theta = 0.7; // defaults to 70% focus on relevance, 30% focus on diversity
    const int worldSize = 1;
    const int worldRank = 0;
    const size_t numberOfDataRows = 0;
    const LoadInput loadInput;
    const GenerateInput generateInput;
} typedef AppData;

#endif