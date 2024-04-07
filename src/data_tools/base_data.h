
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
    const std::vector<std::unique_ptr<DataRow>> data;
    const size_t columns;

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
    const std::vector<std::unique_ptr<DataRow>> data;
    const std::vector<size_t> localRowToGlobalRow;
    const size_t columns;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    SegmentedData(const BaseData&);

    public:
    static std::unique_ptr<SegmentedData> load(
        DataRowFactory &factory, 
        std::istream &source, 
        const std::vector<unsigned int> &rankMapping, 
        const unsigned int rank
    ) {
        size_t columns = 0;
        std::vector<std::unique_ptr<DataRow>> data;
        std::vector<size_t> localRowToGlobalRow;
        size_t globalRow = 0;

        while (true) {
            DataRow* nextRow = factory.maybeGet(source);

            if (nextRow == nullptr) {
                break;
            }

            if (columns == 0) {
                columns = nextRow->size();
            }

            if (rankMapping[globalRow] == rank) {
                data.push_back(std::unique_ptr<DataRow>(nextRow));
                localRowToGlobalRow.push_back(globalRow);
                globalRow++;
            }
        }

        if (localRowToGlobalRow.size() != data.size()) {
            std::cout << "sizes did not match, this is likely a serious error" << std::endl;
        }

        return std::unique_ptr<SegmentedData>(new SegmentedData(move(data), move(localRowToGlobalRow), columns));
    }

    SegmentedData(
        std::vector<std::unique_ptr<DataRow>> raw,
        std::vector<size_t> localRowToGlobalRow,
        size_t columns
    ) : data(move(raw)), localRowToGlobalRow(move(localRowToGlobalRow)), columns(columns) {}

    const DataRow& getRow(size_t i) const {
        return *(this->data[i]);
    }

    size_t totalRows() const {
        return this->data.size();
    }

    size_t totalColumns() const {
        return this->columns;
    }

    size_t getRemoteIndexForRow(const size_t localRowIndex) const {
        return this->localRowToGlobalRow[localRowIndex];
    }
};

class ReceivedData : public BaseData {
    private:
    const std::unique_ptr<std::vector<std::pair<size_t, std::unique_ptr<DataRow>>>> &base;
    const size_t rows;
    const size_t columns;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    ReceivedData(const BaseData&);

    public:
    ReceivedData(std::unique_ptr<std::vector<std::pair<size_t, std::unique_ptr<DataRow>>>> base) 
    : base(move(base)), rows(base->size()), columns(base->at(0).second->size()) {}

    const DataRow& getRow(size_t i) const {
        return *(this->base->at(i).second);
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
            translatedRows.push_back(this->base->at(relativeRow).first);
        }

        return Subset::of(translatedRows, localSolution->getScore());
    }
};