#include <map>
#include <vector>

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
