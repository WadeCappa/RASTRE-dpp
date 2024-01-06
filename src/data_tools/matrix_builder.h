#include <vector>
#include <cstddef>
#include <optional>
#include <cassert>

#define assertm(exp, msg) assert(((void)msg, exp))

struct matrix {
    std::vector<std::vector<double>> data;
    size_t rows;
    size_t columns;
} typedef Matrix;

class MatrixBuilder {
    public:
    static Matrix buildMatrix(DataLoader &loader) {
        std::optional<size_t> columns = std::nullopt;
        Matrix result;
        result.rows = 0;

        std::vector<double> element;
        while (loader.getNext(element)) {
            result.rows++;
            if (!columns.has_value()) {
                columns = element.size();
                result.columns = element.size();
            }

            assertm(result.columns == element.size(), "All rows should have the same number of values.");
            result.data.push_back(element);
        }

        return result;
    }
};