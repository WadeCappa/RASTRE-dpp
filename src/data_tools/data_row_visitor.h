#include <map>
#include <vector>

#ifndef DATA_ROW_VISITOR_H
#define DATA_ROW_VISITOR_H

class DataRowVisitor {
    public:
    virtual ~DataRowVisitor() {}
    virtual void visitDenseDataRow(const std::vector<float>& data) = 0;
    virtual void visitSparseDataRow(const std::map<size_t, float>& data, size_t totalColumns) = 0;
};

template <typename T>
class ReturningDataRowVisitor : public DataRowVisitor {
    public:
    virtual T get() = 0;
};

#endif