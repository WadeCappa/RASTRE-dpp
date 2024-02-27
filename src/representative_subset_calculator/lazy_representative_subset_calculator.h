#include <vector>
#include <stdexcept>
#include <set>
#include <limits>
#include <utility>
#include <queue>
#include <cstdlib>
#include <algorithm>

class LazySubsetCalculator : public SubsetCalculator {
    private:

    struct HeapComparitor {
        bool operator()(const std::pair<size_t, double> a, const std::pair<size_t, double> b) {
            return a.second < b.second;
        }
    };

    public:
    LazySubsetCalculator() {}

    std::unique_ptr<Subset> getApproximationSet(std::unique_ptr<MutableSubset> consumer, const Data &data, size_t k) {
        std::vector<std::pair<size_t, double>> heap;
        for (size_t index = 0; index < data.totalRows(); index++) {
            const auto & d = data.getRow(index);
            MutableSimilarityMatrix matrix(d);
            heap.push_back(std::make_pair(index, matrix.getCoverage()));
        }

        HeapComparitor comparitor;
        std::make_heap(heap.begin(), heap.end(), comparitor);

        MutableSimilarityMatrix matrix;

        double currentScore = 0;

        while (consumer->size() < k) {
            auto top = heap.front();
            std::pop_heap(heap.begin(),heap.end(), comparitor); 
            heap.pop_back();

            MutableSimilarityMatrix tempMatrix(matrix);
            tempMatrix.addRow(data.getRow(top.first));
            double marginal = tempMatrix.getCoverage();

            auto nextElement = heap.front();

            if (marginal >= nextElement.second) {
                double marginalGain = marginal - currentScore;
                consumer->addRow(top.first, marginalGain);
                matrix.addRow(data.getRow(top.first));
                currentScore = marginal;
            } else {
                top.second = marginal;
                heap.push_back(top);
                std::push_heap(heap.begin(),heap.end(), comparitor);
            }
        }

        return MutableSubset::upcast(move(consumer));
    }
};