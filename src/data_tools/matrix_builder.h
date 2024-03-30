#include <vector>
#include <cstddef>
#include <optional>
#include <cassert>

#define assertm(exp, msg) assert(((void)msg, exp))

class Data {
    public:
    virtual const DataRow& getRow(size_t i) const = 0;
    virtual size_t totalRows() const = 0;
    virtual size_t totalColumns() const = 0;
};

class NaiveData : public Data {
    private:
    std::vector<std::unique_ptr<DataRow>> data;
    size_t rows;
    size_t columns;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    NaiveData(const Data&);

    public:
    NaiveData(DataLoader &loader) {
        std::optional<size_t> columns = std::nullopt;
        this->rows = 0;

        DataRow **nextRow;
        while (loader.getNext(nextRow)) {
            std::unique_ptr<DataRow> row(*nextRow);
            this->rows++;
            if (!columns.has_value()) {
                columns = row->size();
                this->columns = row->size();
            }

            data.push_back(move(row));
        }
    }

    // For testing only
    NaiveData(const std::vector<std::vector<double>> &raw, size_t rows, size_t cols) 
    : 
        rows(rows), 
        columns(cols) 
    {
        for (const auto & row : raw) {
            data.push_back(NaiveDataRow(row));
        }
    }

    const DataRow& getRow(size_t i) const {
        return this->data[i];
    }

    size_t totalRows() const {
        return this->rows;
    }

    size_t totalColumns() const {
        return this->columns;
    }

    bool DEBUG_compareData(const std::vector<DataRow> &data) const {
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

    const DataRow& getRow(size_t i) const {
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
    const std::vector<std::pair<size_t, std::unique_ptr<DataRow>>> &base;
    const size_t rows;
    const size_t columns;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    SelectiveData(const Data&);

    public:
    SelectiveData(const std::vector<std::pair<size_t, std::unique_ptr<DataRow>>> &base) 
    : base(base), rows(base.size()), columns(base[0].second.size()) {}

    const DataRow& getRow(size_t i) const {
        return this->base[i].second;
    }

    size_t totalRows() const {
        return this->rows;
    }

    size_t totalColumns() const {
        return this->columns;
    }

    std::unique_ptr<Subset> translateSolution(
        const std::unique_ptr<Subset> localSolution
    ) const {
        std::vector<size_t> translatedRows;
        for (const auto & relativeRow : *localSolution.get()) {
            translatedRows.push_back(this->base[relativeRow].first);
        }

        return Subset::of(translatedRows, localSolution->getScore());
    }
};