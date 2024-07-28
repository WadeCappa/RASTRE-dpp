
#include <vector>
#include <cstddef>
#include <optional>
#include <cassert>
#include <bits/stdc++.h> 


class BaseData {
    public:
    virtual const DataRow& getRow(size_t i) const = 0;
    virtual size_t totalRows() const = 0;
    virtual size_t totalColumns() const = 0;

    double DEBUG_calculateSparsity() const {
        class SparsityCalculatorDataRowVisitor : public ReturningDataRowVisitor<double> {
            private:
            double res;

            public:
            void visitDenseDataRow(const std::vector<double>& _data) {
                this->res = 1;
            }

            void visitSparseDataRow(const std::map<size_t, double>& data, size_t totalColumns) {
                this-> res = data.size() / totalColumns;
            }

            double get() {
                return res;
            }
        };

        size_t rows = this->totalRows();
        std::vector<double> sparsityPerRow(rows);
        for (size_t i = 0; i < rows; i++) {
            SparsityCalculatorDataRowVisitor sparsityVisitor;
            sparsityPerRow[i] = this->getRow(i).visit(sparsityVisitor);
        }

        return std::accumulate(sparsityPerRow.begin(), sparsityPerRow.end(), 0.0) / sparsityPerRow.size(); 
    }
};

class FullyLoadedData : public BaseData {
    private:
    const std::vector<std::unique_ptr<DataRow>> data;
    const size_t columns;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    FullyLoadedData(const BaseData&);

    public:
    static std::unique_ptr<FullyLoadedData> load(DataRowFactory &factory, LineFactory &getter) {
        size_t columns = 0;
        std::vector<std::unique_ptr<DataRow>> data;

        while (true) {
            DataRow* nextRow = factory.maybeGet(getter);
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

    static std::unique_ptr<FullyLoadedData> load(std::vector<std::vector<double>> raw) {
        std::vector<std::unique_ptr<DataRow>> data;
        for (std::vector<double> v : raw) {
            data.push_back(std::unique_ptr<DataRow>(new DenseDataRow(move(v))));
        }

        size_t cols = data[0]->size();

        return std::unique_ptr<FullyLoadedData>(new FullyLoadedData(move(data), cols));
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
        LineFactory &source, 
        const std::vector<unsigned int> &rankMapping, 
        const unsigned int rank
    ) {
        size_t columns = 0;
        std::vector<std::unique_ptr<DataRow>> data;
        std::vector<size_t> localRowToGlobalRow;

        for (size_t globalRow = 0; globalRow < rankMapping.size(); globalRow++) {
            if (rankMapping[globalRow] != rank) {
                factory.skipNext(source);
            } else {
                std::unique_ptr<DataRow> nextRow(factory.maybeGet(source));

                if (nextRow == nullptr) {
                    break;
                }

                if (columns == 0) {
                    columns = nextRow->size();
                }

                data.push_back(move(nextRow));
                localRowToGlobalRow.push_back(globalRow);
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
    std::unique_ptr<std::vector<std::pair<size_t, std::unique_ptr<DataRow>>>> base;
    const size_t rows;
    const size_t columns;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    ReceivedData(const BaseData&);

    public:
    ReceivedData(
        std::unique_ptr<std::vector<std::pair<size_t, std::unique_ptr<DataRow>>>> input
    ) : 
        base(move(input)),
        rows(this->base->size()), 
        columns(this->base->at(0).second->size())
    {}

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
