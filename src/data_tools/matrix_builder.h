#include <vector>
#include <cstddef>
#include <optional>
#include <cassert>

#define assertm(exp, msg) assert(((void)msg, exp))

class Data {
    public:
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

class LocalData : public Data {
    private:
    const Data &base;
    const std::vector<size_t> localRowToGlobalRow;

    std::vector<size_t> getMapping(const Data &base, const std::vector<unsigned int> &rankMapping, const unsigned int rank) {
        std::vector<size_t> res;

        for (size_t remoteIndex = 0; remoteIndex < rankMapping.size(); remoteIndex++) {
            if (rankMapping[remoteIndex] == rank) {
                res.push_back(remoteIndex);
            }
        }

        return res;
    }

    public:
    LocalData(
        const Data &base, 
        const std::vector<unsigned int> &rankMapping, 
        const unsigned int rank
    ) : base(base), localRowToGlobalRow(getMapping(base, rankMapping, rank)) {}

    const std::vector<double>& getRow(size_t i) const {
        return this->base.getRow(i);
    }

    size_t totalRows() const {
        return this->base.totalRows();
    }

    size_t totalColumns() const {
        return this->base.totalColumns();
    }

    size_t getRemoteIndexForRow(const size_t localRowIndex) const {
        return this->localRowToGlobalRow[localRowIndex];
    }
};

class SelectiveData : public Data {
    private:
    const std::vector<std::pair<size_t, std::vector<double>>> &base;
    const size_t rows;
    const size_t columns;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    SelectiveData(const Data&);

    public:
    SelectiveData(const std::vector<std::pair<size_t, std::vector<double>>> &base) 
    : base(base), rows(base.size()), columns(base[0].second.size()) {}

    const std::vector<double>& getRow(size_t i) const {
        return this->base[i].second;
    }

    size_t totalRows() const {
        return this->rows;
    }

    size_t totalColumns() const {
        return this->columns;
    }

    std::unique_ptr<RepresentativeSubset> translateSolution(
        const std::unique_ptr<RepresentativeSubset> localSolution
    ) const {
        std::vector<size_t> translatedRows;
        for (const auto & relativeRow : *localSolution.get()) {
            translatedRows.push_back(this->base[relativeRow].first);
        }

        return RepresentativeSubset::of(translatedRows, localSolution->getScore());
    }
};