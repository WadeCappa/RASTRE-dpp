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
    Timers &timers;

    struct HeapComparitor {
        bool operator()(const std::pair<size_t, double> a, const std::pair<size_t, double> b) {
            return a.second < b.second;
        }
    };

    public:
    LazyRepresentativeSubsetCalculator(Timers &timers) : timers(timers) {}

    std::vector<std::pair<size_t, double>> getApproximationSet(const Data &data, size_t k) {
        timers.totalCalculationTime.startTimer();

        std::vector<std::pair<size_t, double>> subsetRows;
        std::vector<std::pair<size_t, double>> heap;
        for (size_t index = 0; index < data.rows; index++) {
            const auto & d = data.data[index];
            SimilarityMatrix matrix(d);
            heap.push_back(std::make_pair(index, matrix.getCoverage()));
        }

        HeapComparitor comparitor;
        std::make_heap(heap.begin(), heap.end(), comparitor);

        SimilarityMatrix matrix;

        double currentScore = 0;

        while (subsetRows.size() < k) {
            auto top = heap.front();
            std::pop_heap(heap.begin(),heap.end(), comparitor); 
            heap.pop_back();

            SimilarityMatrix tempMatrix(matrix);
            tempMatrix.addRow(data.data[top.first]);
            double marginal = tempMatrix.getCoverage();

            auto nextElement = heap.front();

            if (marginal >= nextElement.second) {
                double marginalGain = marginal - currentScore;
                subsetRows.push_back(std::make_pair(top.first, marginalGain));
                matrix.addRow(data.data[top.first]);

                std::cout << "lazy found " << top.first << " which increasd marginal score by " << marginalGain << std::endl;
                currentScore = marginal;
            } else {
                top.second = marginal;
                heap.push_back(top);
                std::push_heap(heap.begin(),heap.end(), comparitor);
            }
        }

        return subsetRows;
    }
};