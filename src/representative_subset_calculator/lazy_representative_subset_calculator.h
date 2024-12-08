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
        bool operator()(const std::pair<size_t, float> a, const std::pair<size_t, float> b) {
            return a.second < b.second;
        }
    };

    public:
    LazySubsetCalculator() {}

    /**
     * TODO, use the relevance calculator. Right now this class is unfinished
     */
    std::unique_ptr<Subset> getApproximationSet(
        std::unique_ptr<MutableSubset> consumer, 
        std::unique_ptr<RelevanceCalculator> _calc,
        const BaseData &data, 
        size_t k
    ) {
        std::vector<std::pair<size_t, float>> heap;
        for (size_t index = 0; index < data.totalRows(); index++) {
            // TODO: Use the kernel matrix here, keep diagonals as state
            MutableSimilarityMatrix matrix(data.getRow(index));
            heap.push_back(std::make_pair(index, matrix.getCoverage()));
        }

        HeapComparitor comparitor;
        std::make_heap(heap.begin(), heap.end(), comparitor);

        MutableSimilarityMatrix matrix;

        float currentScore = 0;

        while (consumer->size() < k) {
            auto top = heap.front();
            std::pop_heap(heap.begin(),heap.end(), comparitor); 
            heap.pop_back();

            // TODO: Use the kernel matrix here, keep diagonals as state
            MutableSimilarityMatrix tempMatrix(matrix);
            tempMatrix.addRow(data.getRow(top.first));
            float marginal = tempMatrix.getCoverage();

            auto nextElement = heap.front();

            if (marginal >= nextElement.second) {
                float marginalGain = marginal - currentScore;
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