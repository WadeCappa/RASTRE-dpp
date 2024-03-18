#include <vector>
#include <exception>

class MatrixRow {
    public:
    virtual std::vector<double> linearize() const = 0;
};

class DenseMatrixRow : public MatrixRow {
    private:
    std::vector<double> base;

    public:
    double getDotProduct(const std::vector<double> &other) const {
        if (this->base.size() != other.size()) {
            throw new std::invalid_argument("base size and other size were not equal, cannot preform dot product.");
        }

        double res = 0;
        for (size_t i = 0; i < other.size() && i < this->base.size(); i++) {
            res += other[i] * this->base[i];
        }
    
        return res;
    }

    std::vector<double> linearize() const {
        return this->base;
    }
};