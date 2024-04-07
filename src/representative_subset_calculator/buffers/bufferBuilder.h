
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
        const BaseData &data, 
        const Subset &localSolution
    ) {
        // Need to include an additional column that marks the index of the sent row 
        //  as well as an additional double for the sender's local solution total 
        //  marginal gain.
        return (data.totalColumns() + DOUBLES_FOR_ROW_INDEX_PER_COLUMN) * localSolution.size() + DOUBLES_FOR_LOCAL_MARGINAL_PER_BUFFER;
    }

    public:
    static unsigned int buildSendBuffer(
        const SegmentedData &data, 
        const Subset &localSolution, 
        std::vector<double> &buffer
    ) {
        // Need to include an additional column that marks the index of the sent row
        const unsigned int totalSendData = getTotalSendData(dynamic_cast<const BaseData&>(data), localSolution);
        const size_t rowSize = data.totalColumns() + DOUBLES_FOR_ROW_INDEX_PER_COLUMN;
        const size_t numberOfRows = localSolution.size();
        buffer.resize(totalSendData);

        // First value is the local total marginal
        buffer[0] = localSolution.getScore();

        // All buffer indexes need to be offset by 1 to account for the 
        //  total marginal being inserted at the beggining of the send buffer
        #pragma parallel for
        for (size_t localRowIndex = 0; localRowIndex < numberOfRows; localRowIndex++) {
            size_t globalIndex = data.getRemoteIndexForRow(localSolution.getRow(localRowIndex));
            BufferBuilderVisitor visitor(
                localRowIndex, 
                globalIndex, 
                rowSize, 
                DOUBLES_FOR_LOCAL_MARGINAL_PER_BUFFER, 
                DOUBLES_FOR_ROW_INDEX_PER_COLUMN, 
                buffer
            );
            data.getRow(localSolution.getRow(localRowIndex)).visit(visitor);
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
    public:
    virtual std::unique_ptr<Subset> getSolution(
        std::unique_ptr<SubsetCalculator> calculator,
        const size_t k
    ) = 0;
};

class GlobalBufferLoader : public BufferLoader {
    private:
    Timers& timers;
    const std::vector<double> &binaryInput;
    const size_t columnsPerRowInBuffer;
    const std::vector<int> &displacements;
    const size_t worldSize;

    public:
    std::unique_ptr<Subset> getSolution(std::unique_ptr<SubsetCalculator> calculator, const size_t k) {
        this->timers.bufferDecodingTime.startTimer();
        ReceivedData bestRows(move(this->rebuildData()));
        this->timers.bufferDecodingTime.stopTimer();

        timers.globalCalculationTime.startTimer();

        std::unique_ptr<Subset> untranslatedSolution(calculator->getApproximationSet(move(NaiveMutableSubset::makeNew()), bestRows, k));
        std::unique_ptr<Subset> globalResult(bestRows.translateSolution(move(untranslatedSolution)));

        timers.globalCalculationTime.stopTimer();

        std::unique_ptr<Subset> bestLocal = this->getBestLocalSolution();
        if (globalResult->getScore() > bestLocal->getScore()) {
            return globalResult; 
        } else {
            return bestLocal;
        }
    }

    GlobalBufferLoader(
        const std::vector<double> &binaryInput, 
        const size_t columnsPerRowInBuffer,
        const std::vector<int> &displacements,
        Timers &timers
    ) : 
    timers(timers),
    binaryInput(binaryInput), 
    columnsPerRowInBuffer(columnsPerRowInBuffer + DOUBLES_FOR_ROW_INDEX_PER_COLUMN), 
    displacements(displacements),
    worldSize(displacements.size())
    {}

    private:
    std::unique_ptr<std::vector<std::pair<size_t, std::unique_ptr<DataRow>>>> rebuildData() {
        std::vector<size_t> rowOffsets = getRowOffsets();
        const size_t totalExpectedRows = rowOffsets.back();
        const size_t numberOfBytesWithoutLocalMarginals = binaryInput.size() - worldSize * DOUBLES_FOR_LOCAL_MARGINAL_PER_BUFFER;

        std::vector<std::pair<size_t, std::unique_ptr<DataRow>>> *newData = new std::vector<std::pair<size_t, std::unique_ptr<DataRow>>>(totalExpectedRows);

        // Because we're sending local marginals along with the data, the input data is no longer uniform.
        #pragma omp parallel for 
        for (size_t rank = 0; rank < worldSize; rank++) {
            const size_t rowOffset = rowOffsets[rank];
            const size_t rowOffsetForNextRank = rank == worldSize - 1 ? totalExpectedRows : rowOffsets[rank + 1];
            #pragma omp parallel for
            for (size_t currentRow = rowOffset; currentRow < rowOffsetForNextRank; currentRow++) {
                const size_t relativeRow = currentRow - rowOffset;
                const size_t globalRowStart = (columnsPerRowInBuffer * relativeRow) + displacements[rank] + DOUBLES_FOR_LOCAL_MARGINAL_PER_BUFFER;
                DataRow *defaultDenseDataRow = new DenseDataRow(
                    std::vector<double>(
                        // Don't capture the first element, which is the index of the row
                        binaryInput.begin() + globalRowStart + DOUBLES_FOR_ROW_INDEX_PER_COLUMN, 
                        binaryInput.begin() + globalRowStart + columnsPerRowInBuffer
                    )
                );
                newData->at(currentRow) = std::make_pair(
                    binaryInput[globalRowStart],
                    std::unique_ptr<DataRow>(defaultDenseDataRow)
                );
            }
        }
    
        return std::unique_ptr<std::vector<std::pair<size_t, std::unique_ptr<DataRow>>>>(newData);
    }

    std::unique_ptr<Subset> getBestLocalSolution() {
        std::vector<size_t> rows;
        double coverage;
        double localMaxCoverage = -1;
        size_t maxRank = -1;

        for (size_t rank = 0; rank < worldSize; rank++) {
            const size_t rankStart = displacements[rank];

            if (binaryInput[rankStart] >= localMaxCoverage) {
                localMaxCoverage = binaryInput[rankStart];
                maxRank = rank;
            }

        }
         
        coverage = localMaxCoverage;
        // extract best local solution
        const size_t rankStart = displacements[maxRank];
        const size_t rankEnd = maxRank == worldSize - 1 ? binaryInput.size() : displacements[maxRank + 1];
        for (
            size_t rankCursor = rankStart + DOUBLES_FOR_LOCAL_MARGINAL_PER_BUFFER; 
            rankCursor < rankEnd; 
            rankCursor += columnsPerRowInBuffer
        ) {
            rows.push_back(binaryInput[rankCursor]);
        }

        return Subset::of(rows, coverage);
    }

    std::vector<size_t> getRowOffsets() {
        std::vector<size_t> expectedRowsPerRank(worldSize);

        for (size_t rank = 0; rank < worldSize; rank++) {
            const size_t startOfCurrentRank = displacements[rank];
            const size_t startOfNextRank = rank == worldSize - 1 ? binaryInput.size() : displacements[rank + 1];
            const size_t numberOfBytes = startOfNextRank - startOfCurrentRank - DOUBLES_FOR_LOCAL_MARGINAL_PER_BUFFER;
            const size_t expectedRows = getExpectedRows(numberOfBytes, columnsPerRowInBuffer);
            expectedRowsPerRank[rank] = expectedRows;
        }

        std::vector<size_t> rowOffsets;
        size_t totalExpectedRows = 0;
        for (const auto & numRows : expectedRowsPerRank) {
            rowOffsets.push_back(totalExpectedRows);
            totalExpectedRows += numRows;
        }

        rowOffsets.push_back(totalExpectedRows);
        return rowOffsets;
    }

    static size_t getExpectedRows(const size_t numberOfBytes, const size_t columnSize) {
        const size_t expectedRows = std::floor(numberOfBytes / columnSize);
        if (numberOfBytes % columnSize != 0) {
            std::cout << "ERROR: did not get an expected number of values from all gather. Bytes " << numberOfBytes << " and columns " << columnSize << std::endl;
        }

        return expectedRows;
    }
};