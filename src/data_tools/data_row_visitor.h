#include <map>
#include <vector>

class DataRowVisitor {
    public:
    virtual void visitDenseDataRow(const std::vector<double>& data) = 0;
    virtual void visitSparseDataRow(const std::map<size_t, double>& data, size_t totalColumns) = 0;
};