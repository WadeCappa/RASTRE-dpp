#include "orchestrator.h"

class MpiOrchestrator { 
    public:
    static void addMpiCmdOptions(CLI::App &app, AppData &appData) {
        Orchestrator::addCmdOptions(app, appData);
        app.add_option("-n,--numberOfRows", appData.numberOfDataRows, "The number of total rows of data in your input file.")->required();
    }

    static unsigned int getTotalSendData(
        const Data &data, 
        const std::vector<std::pair<size_t, double>> &localSolution
    ) {
        // Need to include an additional column that marks the index of the sent row
        return (data.totalColumns() + 1) * localSolution.size();
    }

    static void buildSendBuffer(
        const Data &data, 
        const std::vector<std::pair<size_t, double>> &localSolution, 
        std::vector<double> &buffer,
        const unsigned int totalData 
    ) {
        // Need to include an additional column that marks the index of the sent row
        size_t rowSize = data.totalColumns() + 1;
        size_t numberOfRows = localSolution.size();
        buffer.resize(totalData);

        #pragma parallel for
        for (size_t rowIndex = 0; rowIndex < numberOfRows; rowIndex++) {
            const auto & row = data.getRow(localSolution[rowIndex].first);
            buffer[rowSize * rowIndex] = localSolution[rowIndex].first;
            for (size_t columnIndex = 1; columnIndex < rowSize; columnIndex++) {
                double v = row[columnIndex - 1];
                buffer[rowSize * rowIndex + columnIndex] = v;
            }
        }
    }

    static void buildReceiveBuffer(
        const std::vector<int> &sendSizes, 
        std::vector<double> &receiveBuffer
    ) {
        size_t totalData = 0;
        for (const auto & d : sendSizes) {
            totalData += d;
        }

        receiveBuffer.resize(totalData);
    }

    static std::vector<int> buildDisplacementBuffer(const std::vector<int> &sendSizes) {
        std::vector<int> res;
        unsigned int seenData = 0;
        for (const auto & s : sendSizes) {
            res.push_back(seenData);
            seenData += s;
        }

        return res;
    }

    static void rebuildData(
        const std::vector<double> &binaryInput, 
        const size_t rawColumnCount,
        std::vector<std::pair<size_t, std::vector<double>>> &newData
    ) {
        const size_t expectedColumns = rawColumnCount + 1;
        const size_t expectedRows = std::floor(binaryInput.size() / expectedColumns);
        if (binaryInput.size() % expectedColumns != 0) {
            std::cout << "ERROR: did not get an expected number of values from all gather" << std::endl;
        }

        newData.clear();
        newData.resize(expectedRows);
        
        #pragma omp parallel for
        for (size_t currentRow = 0; currentRow < expectedRows; currentRow++) {
            newData[currentRow] = std::make_pair(
                binaryInput[expectedColumns * currentRow],
                // Don't capture the first element, which is the index of the row
                std::vector<double>(binaryInput.begin() + (expectedColumns * currentRow) + 1, binaryInput.begin() + (expectedColumns * (currentRow + 1)))
            );
        }
    }

    static nlohmann::json buildOutput(
        const AppData &appData, 
        const std::vector<std::pair<size_t, double>> &solution,
        const Data &data,
        const Timers &timers
    ) {
        nlohmann::json output = Orchestrator::buildOutput(appData, solution, data, timers);
        return output;
    }
};