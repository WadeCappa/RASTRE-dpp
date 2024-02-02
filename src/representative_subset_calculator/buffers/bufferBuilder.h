
class Buffer {
    private:
    protected:
    static const size_t DOUBLES_FOR_LOCAL_MARGINAL_PER_BUFFER = 1;
    static const size_t DOUBLES_FOR_ROW_INDEX_PER_COLUMN = 1;

    public:
};

class BufferBuilder : public Buffer {
    private:
    static unsigned int getTotalSendData(
        const Data &data, 
        const std::vector<std::pair<size_t, double>> &localSolution
    ) {
        // Need to include an additional column that marks the index of the sent row 
        //  as well as an additional double for the sender's local solution total 
        //  marginal gain.
        return (data.totalColumns() + DOUBLES_FOR_ROW_INDEX_PER_COLUMN) * localSolution.size() + DOUBLES_FOR_LOCAL_MARGINAL_PER_BUFFER;
    }

    static double getTotalCoverage(const std::vector<std::pair<size_t, double>> &solution) {
        double totalCoverage = 0;
        for (const auto & s : solution) {
            totalCoverage += s.second;
        }

        return totalCoverage;
    }

    public:
    static unsigned int buildSendBuffer(
        const Data &data, 
        const std::vector<std::pair<size_t, double>> &localSolution, 
        std::vector<double> &buffer
    ) {
        // Need to include an additional column that marks the index of the sent row
        const unsigned int totalSendData = getTotalSendData(data, localSolution);
        const size_t rowSize = data.totalColumns() + DOUBLES_FOR_ROW_INDEX_PER_COLUMN;
        const size_t numberOfRows = localSolution.size();
        buffer.resize(totalSendData);

        // First value is the local total marginal
        buffer[0] = getTotalCoverage(localSolution);

        // All buffer indexes need to be offset by 1 to account for the 
        //  total marginal being inserted at the beggining of the send buffer
        #pragma parallel for
        for (size_t rowIndex = 0; rowIndex < numberOfRows; rowIndex++) {
            const auto & row = data.getRow(localSolution[rowIndex].first);
            buffer[rowSize * rowIndex + DOUBLES_FOR_LOCAL_MARGINAL_PER_BUFFER] = localSolution[rowIndex].first;
            for (size_t columnIndex = DOUBLES_FOR_ROW_INDEX_PER_COLUMN; columnIndex < rowSize; columnIndex++) {
                double v = row[columnIndex - DOUBLES_FOR_ROW_INDEX_PER_COLUMN];
                buffer[rowSize * rowIndex + columnIndex + DOUBLES_FOR_LOCAL_MARGINAL_PER_BUFFER] = v;
            }
        }

        return totalSendData;
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

    static void buildDisplacementBuffer(const std::vector<int> &sendSizes, std::vector<int> &displacements) {
        unsigned int seenData = 0;
        for (const auto & s : sendSizes) {
            displacements.push_back(seenData);
            seenData += s;
        }
    }
};

class BufferLoader : public Buffer {
    private:
    const std::vector<double> &binaryInput;
    const size_t columnsPerRowInBuffer;
    const std::vector<int> &displacements;
    std::unique_ptr<std::vector<std::pair<size_t, std::vector<double>>>> newData;

    public:
    BufferLoader(
        const std::vector<double> &binaryInput, 
        const size_t columnsPerRowInBuffer,
        const std::vector<int> &displacements
    ) : 
    binaryInput(binaryInput), 
    columnsPerRowInBuffer(columnsPerRowInBuffer + DOUBLES_FOR_ROW_INDEX_PER_COLUMN), 
    displacements(displacements),
    newData(rebuildData()) {}

    std::unique_ptr<std::vector<std::pair<size_t, std::vector<double>>>> getNewData() {
        return move(newData);
    }

    private:
    std::unique_ptr<std::vector<std::pair<size_t, std::vector<double>>>> rebuildData() {
        const std::vector<size_t> expectedRowsPerRank = getExpectedRows(binaryInput, columnsPerRowInBuffer, displacements);
        std::vector<size_t> rowOffsets;
        size_t totalExpectedRows = 0;
        for (const auto & numRows : expectedRowsPerRank) {
            rowOffsets.push_back(totalExpectedRows);
            totalExpectedRows += numRows;
        }

        const size_t numberOfBytesWithoutLocalMarginals = binaryInput.size() - displacements.size() * DOUBLES_FOR_LOCAL_MARGINAL_PER_BUFFER;

        std::vector<std::pair<size_t, std::vector<double>>> *newData = new std::vector<std::pair<size_t, std::vector<double>>>(totalExpectedRows);

        // Because we're sending local marginals along with the data, the input data is no longer uniform.
        #pragma omp parallel for 
        for (size_t rank = 0; rank < rowOffsets.size(); rank++) {
            const size_t rowOffset = rowOffsets[rank];
            const size_t rowOffsetForNextRank = rank == rowOffsets.size() - 1 ? totalExpectedRows : rowOffsets[rank + 1];
            #pragma omp parallel for
            for (size_t currentRow = rowOffset; currentRow < rowOffsetForNextRank; currentRow++) {
                const size_t relativeRow = currentRow - rowOffset;
                const size_t globalRowStart = (columnsPerRowInBuffer * relativeRow) + displacements[rank] + DOUBLES_FOR_LOCAL_MARGINAL_PER_BUFFER;
                newData->at(currentRow) = std::make_pair(
                    binaryInput[globalRowStart],
                    // Don't capture the first element, which is the index of the row
                    std::vector<double>(binaryInput.begin() + globalRowStart + DOUBLES_FOR_ROW_INDEX_PER_COLUMN, binaryInput.begin() + globalRowStart + columnsPerRowInBuffer)
                );
            }
        }
    
        return std::unique_ptr<std::vector<std::pair<size_t, std::vector<double>>>>(newData);
    }

    static std::vector<size_t> getExpectedRows(
        const std::vector<double> &binaryInput, 
        const size_t expectedColumns,
        const std::vector<int> &displacements
    ) {
        std::vector<size_t> res(displacements.size());

        for (size_t rank = 0; rank < displacements.size(); rank++) {
            const size_t startOfCurrentRank = displacements[rank];
            const size_t startOfNextRank = rank == displacements.size() - 1 ? binaryInput.size() : displacements[rank + 1];
            const size_t numberOfBytes = startOfNextRank - startOfCurrentRank - DOUBLES_FOR_LOCAL_MARGINAL_PER_BUFFER;
            const size_t expectedRows = getExpectedRows(numberOfBytes, expectedColumns);
            res[rank] = expectedRows;
        }

        return res;
    }

    static size_t getExpectedRows(const size_t numberOfBytes, const size_t columnSize) {
        const size_t expectedRows = std::floor(numberOfBytes / columnSize);
        if (numberOfBytes % columnSize != 0) {
            std::cout << "ERROR: did not get an expected number of values from all gather. Bytes " << numberOfBytes << " and columns " << columnSize << std::endl;
        }

        return expectedRows;
    }
};