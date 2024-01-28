#include <vector>
#include <cstddef>
#include <optional>
#include <cassert>

#define assertm(exp, msg) assert(((void)msg, exp))

class Data {
    public:
    virtual double getTransposeDotProduct(size_t i) const = 0;
    virtual const std::vector<double>& getRow(size_t i) const = 0;
    virtual size_t totalRows() const = 0;
    virtual size_t totalColumns() const = 0;
};

class NaiveData : public Data {
    private:
    std::vector<std::vector<double>> data;
    size_t rows;
    size_t columns;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    NaiveData(const Data&);

    public:
    NaiveData(DataLoader &loader) {
        std::optional<size_t> columns = std::nullopt;
        this->rows = 0;

        data.push_back(std::vector<double>());
        std::vector<double> *nextRow = &data.back();
        while (loader.getNext(*nextRow)) {
            this->rows++;
            if (!columns.has_value()) {
                columns = nextRow->size();
                this->columns = nextRow->size();
            }

            data.push_back(std::vector<double>());
            nextRow = &data.back();
        }

        data.pop_back();
    }

    NaiveData(const std::vector<std::vector<double>> &raw, size_t rows, size_t cols) : data(raw), rows(rows), columns(cols) {}

    double getTransposeDotProduct(size_t i) const {
        
    }

    const std::vector<double>& getRow(size_t i) const {
        return this->data[i];
    }

    size_t totalRows() const {
        return this->rows;
    }

    size_t totalColumns() const {
        return this->columns;
    }

    bool DEBUG_compareData(const std::vector<std::vector<double>> &data) const {
        return this->data == data;
    }
};