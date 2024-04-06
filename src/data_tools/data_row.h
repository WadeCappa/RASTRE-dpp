#include <map>
#include <vector>

static const std::string DELIMETER = ",";

class DataRowVisitor {
    public:
    virtual void visitDenseDataRow(const std::vector<double>& data) = 0;
    virtual void visitSparseDataRow(const std::map<size_t, double>& data) = 0;
};

class DenseDotProductDataRowVisitor : public DataRowVisitor {
    private:
    double res;
    const std::vector<double>& base;

    public:
    DenseDotProductDataRowVisitor(const std::vector<double>& base) : base(base), res(0) {}

    void visitDenseDataRow(const std::vector<double>& data) {
        double result = 0;
        for (size_t i = 0; i < this->base.size() && i < data.size(); i++) {
            res += this->base[i] * data[i];
        }

        this->res = result;
    }

    void visitSparseDataRow(const std::map<size_t, double>& data) {
        double result = 0;
        for (const auto & p : data) {
            result += this->base[p.first] * p.second;
        }

        this->res = result;
    }

    double get() const {
        return this->res;
    }
};

class SparseDotProductDataRowVisitor : public DataRowVisitor {
    private:
    double res;
    const std::map<size_t, double>& base;

    public:
    SparseDotProductDataRowVisitor(const std::map<size_t, double>& base) : base(base), res(0) {}

    void visitDenseDataRow(const std::vector<double>& data) {
        double result = 0;
        for (const auto & p : this->base) {
            result += data[p.first] * p.second;
        }

        this->res = result;
    }

    void visitSparseDataRow(const std::map<size_t, double>& data) {
        double result = 0;
        for (const auto & p : this->base) {
            if (data.find(p.first) != data.end()) {
                result += data.at(p.first) * p.second;
            }
        }

        this->res = result;
    }

    double get() const {
        return this->res;
    }
};

class DataRow {
    public:
    virtual size_t size() const = 0;
    virtual void visit(DataRowVisitor &visitor) const = 0;
    virtual double dotProduct(const DataRow& dataRow) const = 0;
}; 

class DenseDataRow : public DataRow {
    private:
    const std::vector<double> data;

    public:
    DenseDataRow() {}
    
    DenseDataRow(std::vector<double> data) : data(move(data)) {}

    size_t size() const {
        return this->data.size();
    }

    double dotProduct(const DataRow& dataRow) const {
        DenseDotProductDataRowVisitor visitor(this->data);
        dataRow.visit(visitor);
        return visitor.get();
    }

    void visit(DataRowVisitor &visitor) const {
        visitor.visitDenseDataRow(this->data);
    }
};

class SparseDataRow : public DataRow {
    public:
    const size_t totalColumns;
    const std::map<size_t, double> rowToValue;

    SparseDataRow(std::map<size_t, double> map, size_t totalCoumns) : rowToValue(move(map)), totalColumns(totalColumns) {}

    size_t size() const {
        return this->totalColumns;
    }

    double dotProduct(const DataRow& dataRow) const {
        SparseDotProductDataRowVisitor visitor(this->rowToValue);
        dataRow.visit(visitor);
        return visitor.get();
    }

    void visit(DataRowVisitor &visitor) const {
        visitor.visitSparseDataRow(this->rowToValue);
    }
};