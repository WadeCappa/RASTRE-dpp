#include <optional>

class DenseDotProductDataRowVisitor : public DataRowVisitor {
    private:
    std::optional<double> result;
    const std::vector<double>& base;

    public:
    DenseDotProductDataRowVisitor(const std::vector<double>& input) 
    : base(input), result(std::nullopt) {}

    void visitDenseDataRow(const std::vector<double>& data) {
        double dotProduct = 0;
        for (size_t i = 0; i < this->base.size() && i < data.size(); i++) {
            dotProduct += this->base[i] * data[i];
        }

        this->result = dotProduct;
    }

    void visitSparseDataRow(const std::map<size_t, double>& data, size_t _totalColumns) {
        double dotProduct = 0;
        for (const auto & p : data) {
            dotProduct += this->base[p.first] * p.second;
        }

        this->result = dotProduct;
    }

    std::optional<double> get() const {
        return this->result;
    }
};

class SparseDotProductDataRowVisitor : public DataRowVisitor {
    private:
    std::optional<double> result;
    const std::map<size_t, double>& base;

    public:
    SparseDotProductDataRowVisitor(const std::map<size_t, double>& input) 
    : base(input), result(std::nullopt) {}

    void visitDenseDataRow(const std::vector<double>& data) {
        double dotProduct = 0;
        for (const auto & p : this->base) {
            dotProduct += data[p.first] * p.second;
        }

        this->result = dotProduct;
    }

    void visitSparseDataRow(const std::map<size_t, double>& data, size_t _totalColumns) {
        double dotProduct = 0;
        for (const auto & p : this->base) {
            if (data.find(p.first) != data.end()) {
                dotProduct += data.at(p.first) * p.second;
            }
        }

        this->result = dotProduct;
    }

    std::optional<double> get() const {
        return this->result;
    }
};