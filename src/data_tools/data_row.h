#include <map>
#include <vector>

class DataRow {
    public:
    virtual const std::vector<double> getRow() const = 0;
}; 

class NaiveDataRow : public DataRow {
    private:
    const std::vector<double> data;

    public:
    NaiveDataRow() {}
    
    NaiveDataRow(std::vector<double> data) : data(move(data)) {}

    const std::vector<double> getRow() const {
        return data;
    }
};

class MapDataRow : public DataRow {
    public:
    const size_t totalColumns;
    const std::map<size_t, double> rowToValue;

    MapDataRow(std::map<size_t, double> map, size_t totalCoumns) : rowToValue(move(map)), totalColumns(totalColumns) {}

    const std::vector<double> getRow() const {
        std::vector<double> res(this->totalColumns, 0);
        for (auto it = this->rowToValue.begin(); it != this->rowToValue.end(); it++)
        {
            res[it->first] = it->second;
        } 

        return move(res);
    }
};