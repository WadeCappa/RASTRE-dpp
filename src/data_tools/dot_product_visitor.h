
#include <optional>
#include <iostream>

#include "data_row_visitor.h"

#ifndef DOT_PRODUCT_VISITOR_H
#define DOT_PRODUCT_VISITOR_H

class DenseDotProductDataRowVisitor : public ReturningDataRowVisitor<float> {
    private:
    std::optional<float> result;
    const std::vector<float>& base;

    public:
    DenseDotProductDataRowVisitor(const std::vector<float>& input) 
    : base(input), result(std::nullopt) {}

    void visitDenseDataRow(const std::vector<float>& data) {
        float dotProduct = 0;
        for (size_t i = 0; i < this->base.size() && i < data.size(); i++) {
            dotProduct += this->base[i] * data[i];
        }

        this->result = dotProduct;
    }

    void visitSparseDataRow(const std::map<size_t, float>& data, size_t _totalColumns) {
        float dotProduct = 0;
        for (const auto & p : data) {
            dotProduct += this->base[p.first] * p.second;
        }

        this->result = dotProduct;
    }

    float get() {
        return this->result.value();
    }
};

class SparseDotProductDataRowVisitor : public ReturningDataRowVisitor<float> {
    private:
    std::optional<float> result;
    const std::map<size_t, float>& base;

    public:
    SparseDotProductDataRowVisitor(const std::map<size_t, float>& input) 
    : base(input), result(std::nullopt) {}

    void visitDenseDataRow(const std::vector<float>& data) {
        float dotProduct = 0;
        for (const auto & p : this->base) {
            dotProduct += data[p.first] * p.second;
        }

        this->result = dotProduct;
    }

    void visitSparseDataRow(const std::map<size_t, float>& data, size_t _totalColumns) {
        float dotProduct = 0;

        auto baseIterator = this->base.begin();
        auto dataIterator = data.begin();
        while (baseIterator != this->base.end() && dataIterator != data.end()) {
            if (dataIterator->first == baseIterator->first) {
                dotProduct += dataIterator->second * baseIterator->second;
                dataIterator++;
                baseIterator++;
            } else if (dataIterator->first > baseIterator->first) {
                baseIterator++;
            } else  {
                dataIterator++;
            }
        }

        this->result = dotProduct;
    }

    float get() {
        return this->result.value();
    }
};

#endif