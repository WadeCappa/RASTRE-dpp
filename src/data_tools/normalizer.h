#include <math.h>
#include <vector>

class Normalizer {
    private:

    public: 
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