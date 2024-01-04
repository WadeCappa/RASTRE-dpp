#include <math.h>
#include <vector>
#include "data_loader.h"

class Normalizer : public DataLoader {
    private:
    DataLoader &loader;

    public: 
    Normalizer(DataLoader &inputLoader) : loader(inputLoader) {}

    bool getNext(std::vector<double> &result) {
        if (this->loader.getNext(result)) {
            normalize(result);
            return true;
        }

        return false;
    }
    
    // Visible for testing only
    static void normalize(std::vector<double> &row) {
        double scalar = 1.0 / vectorLength(row);
        for (auto & e : row) {
            e = scalar * e;
        }
    }

    // Visible for testing only
    static double vectorLength(const std::vector<double> &v) {
        double res = 0;
        for (const auto & e : v) {
            res += std::pow(e, 2);
        }

        return std::pow(res, 0.5);
    }
};  