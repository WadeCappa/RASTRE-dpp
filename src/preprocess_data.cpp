#include "data_tools/normalizer.h"
#include "data_tools/data_saver.h"
#include <sstream>
#include <CLI/CLI.hpp>

struct appData {
    std::string inputFile;
    std::string outputFile;
    bool binaryInput = false;
    bool binaryOutput = false;
    bool normalizeInput = false;
} typedef AppData;

void addCmdOptions(CLI::App &app, AppData &appData) {
    app.add_option("-i,--input", appData.inputFile, "Path to input file. Should contain data in row vector format.")->required();
    app.add_option("-o,--output", appData.outputFile, "Path to output file.")->required();
    app.add_flag("--loadBinary", appData.binaryInput, "Use this flag if you want to load a binary input file.");
    app.add_flag("--saveAsBinary", appData.binaryOutput, "Use this flag if you want to save your output file as a binary file.");
    app.add_flag("--normalize", appData.normalizeInput, "Use this flag to normalize each input vector.");
}

DataLoader* buildDataLoader(bool readBinary, std::istream &data) {
    return readBinary ? (DataLoader*)(new BinaryDataLoader(data)) : (DataLoader*)(new AsciiDataLoader(data));
}

DataSaver* buildDataSaver(bool writeBinary, DataLoader &data) {
    return writeBinary ? (DataSaver*)(new BinaryDataSaver(data)) : (DataSaver*)(new AsciiDataSaver(data));
}

int main(int argc, char** argv) {
    CLI::App app{"App description"};
    AppData appData;
    addCmdOptions(app, appData);
    CLI11_PARSE(app, argc, argv);

    std::cout << "input file: " << appData.inputFile << ", output file: " << appData.outputFile << ", binary input? " << appData.binaryInput << std::endl;

    std::ifstream inputFile;
    inputFile.open(appData.inputFile);
    DataLoader* dataLoader = buildDataLoader(appData.binaryInput, inputFile);
    if (appData.normalizeInput) {
        dataLoader = new Normalizer(*dataLoader);
    }
    
    std::ofstream outputFile;
    outputFile.open(appData.outputFile);
    DataSaver* dataSaver = buildDataSaver(appData.binaryOutput, *dataLoader);

    dataSaver->save(outputFile);
    
    inputFile.close();
    outputFile.close();
    return 0;
}