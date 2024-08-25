#include <Eigen/Dense>
#include <vector>
#include <math.h>
#include <unordered_set>
#include <unordered_map>
#include <optional>


class SendAllSubsetCalculator : public SubsetCalculator {

    private:



    public: 
    SendAllSubsetCalculator() {}

    std::unique_ptr<Subset> getApproximationSet(
        std::unique_ptr<MutableSubset> consumer, 
        const BaseData &data, 
        size_t k
    ) {

        std::unique_ptr<LazyKernelMatrix> kernelMatrix(LazyKernelMatrix::from(data));
        
        std::vector<float> diagonals = kernelMatrix->getDiagonals();
        

        for (size_t i = 0; i < k; i++) {
            float marginalGain = std::log(diagonals[i]);
            consumer->addRow(i, marginalGain);
        }

        return MutableSubset::upcast(move(consumer));
    }

};