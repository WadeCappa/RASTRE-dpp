#include <vector>
#include <cstddef>
#include <optional>
#include <cassert>

#define assertm(exp, msg) assert(((void)msg, exp))

struct Data {
    std::vector<std::vector<double>> data;
    size_t rows;
    size_t columns;

    Data(DataLoader &loader) {
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

    Data(const std::vector<std::vector<double>> &raw, size_t rows, size_t cols) : data(raw), rows(rows), columns(cols) {}

    private:
    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    Data(const Data&);
};