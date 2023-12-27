#include <vector>

class CovarianceMatrix {
    public:
    void addRow(const std::vector<double> &element);
    double calculateInformation();
};