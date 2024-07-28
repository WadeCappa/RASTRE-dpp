
class ToBinaryVisitor : public ReturningDataRowVisitor<std::vector<double>> {
    private:
    std::vector<double> binary;

    public:
    void visitDenseDataRow(const std::vector<double>& data) {
        binary = data;
    }

    void visitSparseDataRow(const std::map<size_t, double>& data, size_t totalColumns) {
        for (const auto & p : data) {
            binary.push_back(static_cast<double>(p.first));
            binary.push_back(p.second);
        }
    }

    std::vector<double> get() {
        return move(binary);
    }
};
