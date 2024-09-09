#include <Eigen/Dense>
#include <vector>
#include <math.h>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <random>
#include <algorithm>


class SendAllSubsetCalculator {

    private:

    std::vector<size_t> randomOrder;    

    public: 
    SendAllSubsetCalculator(size_t numberOfDataRows) {
        for (size_t i = 0; i < numberOfDataRows; i++) {
            randomOrder.push_back(i);
        }

        std::random_device rd; 
        std::mt19937 g(rd()); 
        std::shuffle(randomOrder.begin(), randomOrder.end(), g);
    }

    std::unique_ptr<Subset> getApproximationSet(
        std::unique_ptr<MutableSubset> consumer, 
        const BaseData &data, 
        size_t numberOfDataRows
    ) {

        std::unique_ptr<LazyKernelMatrix> kernelMatrix(LazyKernelMatrix::from(data));
        
        std::vector<float> diagonals = kernelMatrix->getDiagonals();
        

        for (size_t i = 0; i < numberOfDataRows; i++) {
            float marginalGain = std::log(diagonals[i]);
            consumer->addRow(randomOrder[i], marginalGain);
        }

        return MutableSubset::upcast(move(consumer));
    }

};