#include <vector>
#include <stdexcept>
#include <set>
#include <limits>
#include <utility>
#include <queue>
#include <cstdlib>
#include <algorithm>

class LazyRepresentativeSubsetCalculator : public RepresentativeSubsetCalculator {
    private:

    struct HeapComparitor {
        bool operator()(const std::pair<size_t, double> a, const std::pair<size_t, double> b) {
            return a.second < b.second;
        }
    };

    public:
    LazyRepresentativeSubsetCalculator() {}

    std::unique_ptr<RepresentativeSubset> getApproximationSet(const Data &data, size_t k) {
        MutableRepresentativeSubset* solution = new MutableRepresentativeSubset();
        std::vector<std::pair<size_t, double>> heap;
        for (size_t index = 0; index < data.totalRows(); index++) {
            const auto & d = data.getRow(index);
            SimilarityMatrix matrix(d);
            heap.push_back(std::make_pair(index, matrix.getCoverage()));
        }

        HeapComparitor comparitor;
        std::make_heap(heap.begin(), heap.end(), comparitor);

        SimilarityMatrix matrix;

        double currentScore = 0;

        while (solution->size() < k) {
            auto top = heap.front();
            std::pop_heap(heap.begin(),heap.end(), comparitor); 
            heap.pop_back();

            SimilarityMatrix tempMatrix(matrix);
            tempMatrix.addRow(data.getRow(top.first));
            double marginal = tempMatrix.getCoverage();

            auto nextElement = heap.front();

            if (marginal >= nextElement.second) {
                double marginalGain = marginal - currentScore;
                solution->addRow(top.first, marginalGain);
                matrix.addRow(data.getRow(top.first));
                currentScore = marginal;
            } else {
                top.second = marginal;
                heap.push_back(top);
                std::push_heap(heap.begin(),heap.end(), comparitor);
            }
        }

        return MutableRepresentativeSubset::upcast(solution);
    }
};