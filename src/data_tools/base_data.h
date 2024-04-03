
#include <vector>
#include <cstddef>
#include <optional>
#include <cassert>


class BaseData {
    public:
    virtual const DataRow& getRow(size_t i) const = 0;
    virtual size_t totalRows() const = 0;
    virtual size_t totalColumns() const = 0;
};

class FullyLoadedData : public BaseData {
    private:
    std::vector<std::unique_ptr<DataRow>> data;
    size_t columns;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    FullyLoadedData(const BaseData&);

    public:
    static std::unique_ptr<FullyLoadedData> load(DataRowFactory &factory, std::istream &source) {
        size_t columns = 0;
        std::vector<std::unique_ptr<DataRow>> data;

        while (true) {
            DataRow* nextRow = factory.maybeGet(source);
            if (nextRow == nullptr) {
                break;
            }
            if (columns == 0) {
                columns = nextRow->size();
            }

            data.push_back(std::unique_ptr<DataRow>(nextRow));
        }

        return std::unique_ptr<FullyLoadedData>(new FullyLoadedData(move(data), columns));
    }

    FullyLoadedData(std::vector<std::unique_ptr<DataRow>> raw, size_t cols) : data(move(raw)), columns(cols) {}

    const DataRow& getRow(size_t i) const {
        return *(this->data[i]);
    }

    size_t totalRows() const {
        return this->data.size();
    }

    size_t totalColumns() const {
        return this->columns;
    }
};

class SegmentedData : public BaseData {
    private:
    const BaseData &base;
    const std::vector<size_t> localRowToGlobalRow;

    std::vector<size_t> getMapping(const BaseData &base, const std::vector<unsigned int> &rankMapping, const unsigned int rank) {
        std::vector<size_t> res;

        for (size_t remoteIndex = 0; remoteIndex < rankMapping.size(); remoteIndex++) {
            if (rankMapping[remoteIndex] == rank) {
                res.push_back(remoteIndex);
            }
        }

        return res;
    }

    public:
    SegmentedData(
        const BaseData &base, 
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

class ReceivedData : public BaseData {
    private:
    const std::vector<std::pair<size_t, std::unique_ptr<DataRow>>> &base;
    const size_t rows;
    const size_t columns;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    ReceivedData(const BaseData&);

    public:
    ReceivedData(std::vector<std::pair<size_t, std::unique_ptr<DataRow>>> base) 
    : base(move(base)), rows(base.size()), columns(base[0].second->size()) {}

    const DataRow& getRow(size_t i) const {
        return *(this->base[i].second);
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