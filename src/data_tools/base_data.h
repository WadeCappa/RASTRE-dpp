
#include <vector>
#include <omp.h>
#include <cstddef>
#include <optional>
#include <cassert>
#include <bits/stdc++.h> 

#include "data_row.h"
#include "data_row_visitor.h"
#include "data_row_factory.h"
#include "../representative_subset_calculator/representative_subset.h"

#ifndef BASE_DATA_H
#define BASE_DATA_H

struct diagnostics {
    float sparsity;
    size_t numberOfNonEmptyCells;
} typedef Diagnostics;

class BaseData {
    public:
    virtual ~BaseData() {}
    /**
     * there is a semantic bug with this method that needs to get fixed. This 
     * method incorrectly assumes that local rows will be used instead of global
     * rows. We should eradicate the concept of local rows from this concept 
     * and always use global rows for sanity reasons
     */
    virtual const DataRow& getRow(size_t i) const = 0;
    virtual size_t totalRows() const = 0;
    virtual size_t totalColumns() const = 0;

    virtual size_t getRemoteIndexForRow(const size_t localRowIndex) const = 0; 
    virtual size_t getLocalIndexFromGlobalIndex(const size_t globalIndex) const = 0;

    void print_DEBUG() const {
        class PrintVisitor : public DataRowVisitor {
            public:
            void visitDenseDataRow(const std::vector<float>& data) {
                for (const auto & v : data) {
                    std::cout << v << " ";
                }
                std::cout << std::endl;
            }

            void visitSparseDataRow(const std::map<size_t, float>& data, size_t totalColumns) {
                for (size_t i = 0; i < totalColumns; i++) {
                    const float val = data.find(i) == data.end() ? 0.0 : data.at(i);
                    std::cout << val << " ";
                }
                std::cout << std::endl;
            }
        };

        PrintVisitor v;
        const size_t r = this->totalRows();
        for (size_t i = 0; i < r; i++) {
            this->getRow(i).voidVisit(v);
        }
    }

    Diagnostics DEBUG_getDiagnostics() const {
        size_t rows = this->totalRows();
        if (rows == 0) {
            return Diagnostics{0, 0};
        }

        class DiagnosticsVisitor : public ReturningDataRowVisitor<Diagnostics> {
            private:
            std::vector<float> sparsity;
            size_t totalNonEmptyCells;
            size_t i;
            
            public:
            DiagnosticsVisitor(size_t rows) 
            : sparsity(rows), i(0), totalNonEmptyCells(0) {}

            void visitDenseDataRow(const std::vector<float>& data) {
                sparsity[i++] = 0.0;
                totalNonEmptyCells += data.size();
            }

            void visitSparseDataRow(const std::map<size_t, float>& data, size_t totalColumns) {
                sparsity[i++] = (float)((float)(totalColumns - data.size()) / (float)totalColumns);
                totalNonEmptyCells += data.size();
            }

            Diagnostics get() {
                return Diagnostics{
                    std::accumulate(sparsity.begin(), sparsity.end(), (float)0.0) / sparsity.size(),
                    totalNonEmptyCells
                };
            }
        };

        DiagnosticsVisitor visitor(rows);
        for (size_t i = 0; i < rows; i++) {
            this->getRow(i).voidVisit(visitor);
        }

        return visitor.get();
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
            std::unique_ptr<DataRow> nextRow(factory.maybeGet(getter));
            if (nextRow == nullptr) {
                break;
            }
            if (columns == 0) {
                columns = nextRow->size();
            }

            data.push_back(std::move(nextRow));
        }

        return std::unique_ptr<FullyLoadedData>(new FullyLoadedData(std::move(data), columns));
    }

    static std::unique_ptr<FullyLoadedData> load(std::vector<std::vector<float>> raw) {
        std::vector<std::unique_ptr<DataRow>> data;
        for (std::vector<float> v : raw) {
            data.push_back(std::unique_ptr<DataRow>(new DenseDataRow(std::move(v))));
        }

        size_t cols = data[0]->size();

        return std::unique_ptr<FullyLoadedData>(new FullyLoadedData(std::move(data), cols));
    }

    std::unique_ptr<FullyLoadedData> withoutColumns(const std::unordered_set<size_t>& columnsToRemove) {

        class DropColumnsVisitor : public ReturningDataRowVisitor<std::unique_ptr<DataRow>> {
            private:
            const std::unordered_set<size_t>& columnsToRemove;
            std::unique_ptr<DataRow> newRow = nullptr;

            public:
            DropColumnsVisitor(const std::unordered_set<size_t>& columnsToRemove) : columnsToRemove(columnsToRemove) {}

            std::unique_ptr<DataRow> get() {
                return std::move(newRow);
            }

            void visitDenseDataRow(const std::vector<float>& data) {
                std::vector<float> newData;
                for (size_t i = 0; i < data.size(); i++) {
                    if (columnsToRemove.find(i) == columnsToRemove.end()) {
                        newData.push_back(data[i]);
                    }
                }

                newRow = DenseDataRow::of(std::move(newData));
            }

            void visitSparseDataRow(const std::map<size_t, float>& data, size_t totalColumns) {
                std::map<size_t, float> newData;
                for (const auto & d : data) {
                    if (columnsToRemove.find(d.first) == columnsToRemove.end()) {
                        newData.insert({d.first, d.second});
                    }
                }

                newRow = SparseDataRow::of(std::move(newData), totalColumns - columnsToRemove.size());
            }
        };

        std::vector<std::unique_ptr<DataRow>> newRows;
        DropColumnsVisitor v(columnsToRemove);

        for (const std::unique_ptr<DataRow>& d : data) {
            newRows.push_back(d->visit(v));
        }

        return std::unique_ptr<FullyLoadedData>(new FullyLoadedData(std::move(newRows), columns - columnsToRemove.size()));
    }

    FullyLoadedData(std::vector<std::unique_ptr<DataRow>> raw, size_t cols) : data(std::move(raw)), columns(cols) {}

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
        return localRowIndex;
    }

    size_t getLocalIndexFromGlobalIndex(const size_t globalIndex) const {
        return globalIndex;
    }
};

class LoadedSegmentedData : public BaseData {
    private:
    const std::vector<std::unique_ptr<DataRow>> data;
    const std::vector<size_t> localRowToGlobalRow;
    /**
     * this is a hack, we should not need to maintain this datastructure, refactor needed.
     * We're logically coupled to local rows which should not be a thing
     */
    const std::unordered_map<size_t, size_t> globalRowToLocalRow;
    const size_t columns;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    LoadedSegmentedData(const BaseData&);

    public:
    static std::unique_ptr<BaseData> loadInParallel(
        DataRowFactory &factory, 
        GeneratedLineFactory &getter, 
        const std::vector<unsigned int> &rankMapping, 
        const unsigned int rank) {

        std::vector<size_t> localRowToGlobalRow;
        std::unordered_map<size_t, size_t> globalRowToLocalRow;
        for (size_t globalRow = 0; globalRow < rankMapping.size(); globalRow++) {
            if (rankMapping[globalRow] == rank) {
                globalRowToLocalRow.insert({globalRow, localRowToGlobalRow.size()});
                localRowToGlobalRow.push_back(globalRow);
            }
        }

        std::vector<std::unique_ptr<DataRow>> data(localRowToGlobalRow.size());

        std::vector<std::unique_ptr<GeneratedLineFactory>> gettersForRanks;
        std::vector<std::unique_ptr<DataRowFactory>> factoriesForRanks;
        int maxThreads = omp_get_max_threads();
        for (int i = 0; i < maxThreads; i++) {
            gettersForRanks.push_back(getter.copy());
            factoriesForRanks.push_back(factory.copy());
        }

        #pragma omp parallel for 
        for (size_t i = 0; i < localRowToGlobalRow.size(); i++) {
            const int threadRank = omp_get_thread_num();
            GeneratedLineFactory &localGetter(*gettersForRanks[threadRank]);
            DataRowFactory &localFactory(*factoriesForRanks[threadRank]);

            localGetter.jumpToLine(localRowToGlobalRow[i]);
            localFactory.jumpToLine(localRowToGlobalRow[i]);
            std::unique_ptr<DataRow> nextRow(localFactory.maybeGet(localGetter));
            if (nextRow == nullptr) {
                throw std::invalid_argument("Retrieved nullptr which is unexpected in a parellel load. The number of rows you have provided was incorrect.");
            }

            data[i] = std::move(nextRow);
        }

        const size_t columns = localRowToGlobalRow.size() > 0 ? data.back()->size() : 0;
        return std::unique_ptr<BaseData>(
            new LoadedSegmentedData(std::move(data), std::move(localRowToGlobalRow), std::move(globalRowToLocalRow), columns)
        );
    }

    static std::unique_ptr<BaseData> load(
        DataRowFactory &factory, 
        LineFactory &source, 
        const std::vector<unsigned int> &rankMapping, 
        const unsigned int rank
    ) {
        size_t columns = 0;
        std::vector<std::unique_ptr<DataRow>> data;
        std::vector<size_t> localRowToGlobalRow;
        std::unordered_map<size_t, size_t> globalRowToLocalRow;

        for (size_t globalRow = 0; globalRow < rankMapping.size(); globalRow++) {
            if (rankMapping[globalRow] != rank) {
                factory.skipNext(source);
            } else {
                std::unique_ptr<DataRow> nextRow(factory.maybeGet(source));

                if (nextRow == nullptr) {
                    throw std::invalid_argument("Retrieved nullptr which is unexpected during a multi-machine load. The number of rows you have provided was incorrect.");
                }

                if (columns == 0) {
                    columns = nextRow->size();
                }

                data.push_back(std::move(nextRow));
                globalRowToLocalRow.insert({globalRow, localRowToGlobalRow.size()});
                localRowToGlobalRow.push_back(globalRow);
            }
        }

        if (localRowToGlobalRow.size() != data.size()) {
            spdlog::error("sizes did not match, this is likely a serious error");
        }

        return std::unique_ptr<BaseData>(
            new LoadedSegmentedData(std::move(data), std::move(localRowToGlobalRow), std::move(globalRowToLocalRow), columns)
        );
    }

    LoadedSegmentedData(
        std::vector<std::unique_ptr<DataRow>> raw,
        std::vector<size_t> localRowToGlobalRow,
        std::unordered_map<size_t, size_t> globalRowToLocalRow,
        size_t columns
    ) : data(std::move(raw)), localRowToGlobalRow(std::move(localRowToGlobalRow)), globalRowToLocalRow(std::move(globalRowToLocalRow)), columns(columns) {}

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

    size_t getLocalIndexFromGlobalIndex(const size_t globalIndex) const {
        if (globalRowToLocalRow.find(globalIndex) == globalRowToLocalRow.end()) {
            spdlog::error("failed mapping for {0:d} out of {1:d} possible mappings", globalIndex, globalRowToLocalRow.size());
        }
        return globalRowToLocalRow.at(globalIndex);
    }
};

class ReceivedData : public BaseData {
    private:
    std::unique_ptr<std::vector<std::pair<size_t, std::unique_ptr<DataRow>>>> base;
    const std::unordered_map<size_t, size_t> globalRowToLocalRow;
    const size_t rows;
    const size_t columns;

    // Disable pass by value. This object is too large for pass by value to make sense implicitly.
    //  Use an explicit constructor to pass by value.
    ReceivedData(const BaseData&);

    public:
    static std::unique_ptr<ReceivedData> create(
        std::unique_ptr<std::vector<std::pair<size_t, std::unique_ptr<DataRow>>>> input
    ) {
        std::unordered_map<size_t, size_t> globalRowToLocalRow;
        size_t i = 0;
        for (const std::pair<size_t, std::unique_ptr<DataRow>> & p : *input) {
            globalRowToLocalRow.insert({p.first, i++});
        }

        return std::unique_ptr<ReceivedData>(
            new ReceivedData(std::move(input), std::move(globalRowToLocalRow))
        );
    }

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

    size_t getRemoteIndexForRow(const size_t localRowIndex) const {
        return base->at(localRowIndex).first;
    }

    size_t getLocalIndexFromGlobalIndex(const size_t globalIndex) const {
        return globalRowToLocalRow.at(globalIndex);
    }
    
    private:
    ReceivedData(
        std::unique_ptr<std::vector<std::pair<size_t, std::unique_ptr<DataRow>>>> input,
        std::unordered_map<size_t, size_t> globalRowToLocalRow
    ) : 
        base(std::move(input)),
        globalRowToLocalRow(std::move(globalRowToLocalRow)),
        rows(this->base->size()), 
        columns(this->base->at(0).second->size())
    {}
};

#endif