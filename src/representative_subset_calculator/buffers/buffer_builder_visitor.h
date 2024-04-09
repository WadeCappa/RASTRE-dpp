

class BufferBuilderVisitor : public DataRowVisitor {
    private:
    const size_t localRowIndex;
    const size_t globalRowIndex;
    const size_t rowSize;
    const size_t doublesForLocalMarginalPerBuffer;
    const size_t doublesForRowIndexPerColumn;

    std::vector<double> &buffer;

    public:
    BufferBuilderVisitor(
        size_t localRowIndex, 
        size_t globalRowIndex,
        size_t rowSize,
        size_t doublesForLocalMarginalPerBuffer,
        size_t doublesForRowIndexPerColumn,
        std::vector<double> &buffer
    ) : 
        localRowIndex(localRowIndex), 
        globalRowIndex(globalRowIndex),
        rowSize(rowSize),
        doublesForLocalMarginalPerBuffer(doublesForLocalMarginalPerBuffer),
        doublesForRowIndexPerColumn(doublesForRowIndexPerColumn),
        buffer(buffer) 
    {}

    void visitDenseDataRow(const std::vector<double>& data) {
        this->buffer[
            this->rowSize * this->localRowIndex + this->doublesForLocalMarginalPerBuffer
        ] = globalRowIndex;

        for (
            size_t columnIndex = this->doublesForRowIndexPerColumn;
            columnIndex < this->rowSize;
            columnIndex++
        ) {
            double v = data[columnIndex - this->doublesForRowIndexPerColumn];
            this->buffer[this->rowSize * this->localRowIndex + columnIndex + this->doublesForLocalMarginalPerBuffer] = v;
        }
    }

    void visitSparseDataRow(const std::map<size_t, double>& data, size_t totalColumns) {
        size_t index = 0;
        for (const auto & p : data) {
            buffer[index++] = p.first;
            buffer[index++] = p.second;
        }

        buffer[index] = CommunicationConstants::getNoMoreEdgesTag();
    }
};