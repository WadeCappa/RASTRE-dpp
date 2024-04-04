#include <map>
#include <vector>

static const std::string DELIMETER = ",";
class DataRow {
    public:
    virtual const std::vector<double> getRow() const = 0;
    virtual double dotProduct(const DataRow& dataRow) const = 0;
    virtual size_t size() const = 0;
}; 

class DenseDataRow : public DataRow {
    private:
    const std::vector<double> data;

    public:
    DenseDataRow() {}
    
    DenseDataRow(std::vector<double> data) : data(move(data)) {}

    const std::vector<double> getRow() const {
        return data;
    }

    size_t size() const {
        return this->data.size();
    }
};

class SparseDataRow : public DataRow {
    public:
    const size_t totalColumns;
    const std::map<size_t, double> rowToValue;

    SparseDataRow(std::map<size_t, double> map, size_t totalCoumns) : rowToValue(move(map)), totalColumns(totalColumns) {}

    const std::vector<double> getRow() const {
        std::vector<double> res(this->totalColumns, 0);
        for (auto it = this->rowToValue.begin(); it != this->rowToValue.end(); it++)
        {
            res[it->first] = it->second;
        } 

        return move(res);
    }

    size_t size() const {
        return this->totalColumns;
    }
};