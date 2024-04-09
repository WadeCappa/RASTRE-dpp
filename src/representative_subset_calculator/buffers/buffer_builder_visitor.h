

class BufferBuilderVisitor : public DataRowVisitor {
    private:
    const size_t start;

    std::vector<double> &buffer;

    public:
    BufferBuilderVisitor(
        const size_t start,
        std::vector<double> &buffer
    ) : 
        start(start),
        buffer(buffer) 
    {}

    void visitDenseDataRow(const std::vector<double>& data) {
        size_t b = start;
        for (size_t i = 0; i < data.size(); i++) {
            this->buffer[b++] = data[i];
        }
    }

    void visitSparseDataRow(const std::map<size_t, double>& data, size_t totalColumns) {
        for (const auto & p : data) {
            buffer[p.first] = p.second;
        }
    }
};