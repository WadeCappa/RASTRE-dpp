
#include "data_row_visitor.h"

#ifndef TO_BINARY_VISITOR_H
#define TO_BINARY_VISITOR_H

class ToBinaryVisitor : public ReturningDataRowVisitor<std::vector<float>> {
    private:
    std::vector<float> binary;

    public:
    void visitDenseDataRow(const std::vector<float>& data) {
        binary = data;
    }

    void visitSparseDataRow(const std::map<size_t, float>& data, size_t totalColumns) {
        for (const auto & p : data) {
            binary.push_back(static_cast<float>(p.first));
            binary.push_back(p.second);
        }
    }

    std::vector<float> get() {
        return move(binary);
    }
};

#endif