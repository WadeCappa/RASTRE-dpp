
class Buffer {
    private:
    protected:
    static const size_t FLOATS_FOR_LOCAL_MARGINAL_PER_BUFFER = 1;
    static const size_t FLOATS_FOR_ROW_INDEX_PER_COLUMN = 1;

    public:
    virtual ~Buffer() {}
};

class BufferBuilder : public Buffer {
    private:
    public:
    static unsigned int buildSendBuffer(
        const BaseData &data, 
        const Subset &localSolution, 
        std::vector<float> &buffer
    ) {
        // Need to include an additional column that marks the index of the sent row
        std::vector<std::vector<float>> buffers(localSolution.size());

        #pragma omp parallel for
        for (size_t localRowIndex = 0; localRowIndex < localSolution.size(); localRowIndex++) {
            ToBinaryVisitor v;
            buffers[localRowIndex] = data.getRow(localSolution.getRow(localRowIndex)).visit(v);
            buffers[localRowIndex].push_back(data.getRemoteIndexForRow(localSolution.getRow(localRowIndex)));
            buffers[localRowIndex].push_back(CommunicationConstants::endOfSendTag());
        }

        size_t totalSend = 1;
        for (const auto & b : buffers) {
            totalSend += b.size();
        }

        buffer.resize(totalSend, 0);
        buffer[0] = localSolution.getScore();

        size_t i = 1;
        for (const auto & b : buffers) {
            for (const auto d : b) {
                buffer[i++] = d;
            }
        }

        return totalSend;
    }

    static void buildReceiveBuffer(
        const std::vector<int> &sendSizes, 
        std::vector<float> &receiveBuffer
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
        const size_t k,
        const DataRowFactory &factory
    ) = 0;
};

class GlobalBufferLoader : public BufferLoader {
    private:
    Timers& timers;
    const std::vector<float> &binaryInput;
    const size_t columnsPerRowInBuffer;
    const std::vector<int> &displacements;
    const size_t worldSize;
    const RelevanceCalculatorFactory &calcFactory;

    public:
    std::unique_ptr<Subset> getSolution(
        std::unique_ptr<SubsetCalculator> calculator, 
        const size_t k,
        const DataRowFactory &factory
    ) {
        this->timers.bufferDecodingTime.startTimer();
        ReceivedData bestRows(move(this->rebuildData(factory)));
        this->timers.bufferDecodingTime.stopTimer();

        timers.globalCalculationTime.startTimer();

        std::unique_ptr<RelevanceCalculator> calc(calcFactory.build(bestRows));
        std::unique_ptr<Subset> untranslatedSolution(calculator->getApproximationSet(
            NaiveMutableSubset::makeNew(), *calc, bestRows, k)
        );
        std::unique_ptr<Subset> globalResult(bestRows.translateSolution(move(untranslatedSolution)));

        timers.globalCalculationTime.stopTimer();

        std::unique_ptr<Subset> bestLocal = this->getBestLocalSolution();

        spdlog::info("best local solution had score of {0:f} while the global solution had a score of {1:f}", bestLocal->getScore(), globalResult->getScore());
        if (globalResult->getScore() > bestLocal->getScore()) {
            return move(globalResult); 
        } else {
            return move(bestLocal);
        }
    }

    GlobalBufferLoader(
        const std::vector<float> &binaryInput, 
        const size_t columnsPerRowInBuffer,
        const std::vector<int> &displacements,
        Timers &timers,
        const RelevanceCalculatorFactory &calcFactory
    ) : 
        timers(timers),
        binaryInput(binaryInput), 
        columnsPerRowInBuffer(columnsPerRowInBuffer + FLOATS_FOR_ROW_INDEX_PER_COLUMN), 
        displacements(displacements),
        worldSize(displacements.size()),
        calcFactory(calcFactory)
    {}

    private:
    std::unique_ptr<std::vector<std::pair<size_t, std::unique_ptr<DataRow>>>> rebuildData(
        const DataRowFactory &factory
    ) {

        spdlog::debug("rebuilding data for evaluation");
        std::vector<std::vector<std::pair<size_t, std::unique_ptr<DataRow>>>> tempData(worldSize);
    
        // Because we're sending local marginals along with the data, the input data is no longer uniform.
        #pragma omp parallel for 
        for (size_t rank = 0; rank < worldSize; rank++) {
            std::vector<std::pair<size_t, std::unique_ptr<DataRow>>> rankData;
            const size_t rankStart = displacements[rank] + 1;
            const auto rankStop = (rank + 1) == worldSize ? binaryInput.end() : binaryInput.begin() + displacements[rank + 1];

            auto index = binaryInput.begin() + rankStart;
            auto elementStop = binaryInput.begin() + rankStart;
            while (index < rankStop && elementStop < rankStop) {
                if (*elementStop == CommunicationConstants::endOfSendTag()) {
                    std::unique_ptr<DataRow> dataRow(factory.getFromNaiveBinary(
                        move(std::vector<float>(index, elementStop - 1)))
                    );

                    const size_t globalTag = *(elementStop - 1);

                    elementStop++;
                    index = elementStop;

                    rankData.push_back(std::make_pair(globalTag, move(dataRow)));
                } else {
                    elementStop++;
                }
            }

            tempData[rank] = move(rankData);
        }
    

        std::vector<std::pair<size_t, std::unique_ptr<DataRow>>> *newData = new std::vector<std::pair<size_t, std::unique_ptr<DataRow>>>();
        for (auto & r : tempData) {
            for (auto & d : r) {
                newData->push_back(move(d));
            }
        }
        return std::unique_ptr<std::vector<std::pair<size_t, std::unique_ptr<DataRow>>>>(newData);
    }

    std::unique_ptr<Subset> getBestLocalSolution() {
        std::vector<size_t> rows;
        float coverage;
        float bestRankScore = -1;
        size_t maxRank = -1;

        for (size_t rank = 0; rank < worldSize; rank++) {
            const size_t scoreIndex = displacements[rank];
            const float localRankScore = binaryInput[scoreIndex];
            spdlog::info("rank {0:d} had local solution of score {1:f}", rank, localRankScore);

            if (localRankScore >= bestRankScore) {
                bestRankScore = localRankScore;
                maxRank = rank;
            }
        }
         
        // extract best local solution
        const size_t rankStart = displacements[maxRank];
        const size_t rankEnd = maxRank == worldSize - 1 ? binaryInput.size() : displacements[maxRank + 1];
        for (size_t i = rankStart; i < rankEnd; i++) {
            if (binaryInput[i] == CommunicationConstants::endOfSendTag()) {
                rows.push_back(binaryInput[i - 1]);
            }
        }

        return Subset::of(rows, bestRankScore);
    }
};
