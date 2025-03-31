#include <map>
#include <vector>
#include <iostream>
#include <memory>

#include "data_row_visitor.h"
#include "dot_product_visitor.h"

#ifndef DATA_ROW_H
#define DATA_ROW_H

static const std::string DELIMETER = ",";

class DataRow {
    public:
    virtual size_t size() const = 0;
    virtual float dotProduct(const DataRow& dataRow) const = 0;
    virtual void voidVisit(DataRowVisitor &visitor) const = 0;
    virtual ~DataRow() {}

    template <typename T>
    T visit(ReturningDataRowVisitor<T>& visitor) const {
        this->voidVisit(visitor);
        return visitor.get();
    }
}; 

class DenseDataRow : public DataRow {
    private:
    std::vector<float> data;

    DenseDataRow(const DenseDataRow &);

    public:
    DenseDataRow() {}
    
    DenseDataRow(std::vector<float> input) : data(std::move(input)) {}

    static std::unique_ptr<DenseDataRow> of(std::vector<float> data) {
        return std::unique_ptr<DenseDataRow>(new DenseDataRow(std::move(data)));
    }

    void push_back(float val) {
        this->data.push_back(val);
    }

    size_t size() const {
        return this->data.size();
    }

    float dotProduct(const DataRow& dataRow) const {
        DenseDotProductDataRowVisitor visitor(this->data);
        return dataRow.visit(visitor);
    }

    void voidVisit(DataRowVisitor &visitor) const {
        visitor.visitDenseDataRow(this->data);
    }
};

class SparseDataRow : public DataRow {
    private:
    const std::map<size_t, float> rowToValue;
    const size_t totalColumns;

    SparseDataRow(const SparseDataRow &);

    public:
    ~SparseDataRow() {}
    SparseDataRow(std::map<size_t, float> map, size_t totalColumns) : 
        rowToValue(std::move(map)),
        totalColumns(totalColumns) 
    {}

    static std::unique_ptr<SparseDataRow> of(std::map<size_t, float> map, size_t totalColumns) {
        return std::unique_ptr<SparseDataRow>(new SparseDataRow(std::move(map), totalColumns));
    }

    size_t size() const {
        return this->totalColumns;
    }

    float dotProduct(const DataRow& dataRow) const {
        SparseDotProductDataRowVisitor visitor(rowToValue);
        return dataRow.visit(visitor);
    }

    void voidVisit(DataRowVisitor &visitor) const {
        visitor.visitSparseDataRow(rowToValue, this->totalColumns);
    }
};

#endif