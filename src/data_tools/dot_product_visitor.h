#include <optional>
#include <iostream>

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

        auto baseIterator = this->base.begin();
        auto dataIterator = data.begin();
        while (baseIterator != this->base.end() && dataIterator != data.end()) {
            if (dataIterator->first == baseIterator->first) {
                dotProduct += dataIterator->second * baseIterator->second;
                dataIterator++;
                baseIterator++;
            } else if (dataIterator->first > baseIterator->second) {
                baseIterator++;
            } else  {
                dataIterator++;
            }
        }

        this->result = dotProduct;
    }

    std::optional<double> get() const {
        return this->result;
    }
};