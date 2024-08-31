#include <Eigen/Dense>
#include <vector>
#include <math.h>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <random>
#include <algorithm>


class SendAllSubsetCalculator : public SubsetCalculator {

    private:

    std::vector<size_t> randomOrder;    

    public: 
    SendAllSubsetCalculator(size_t numberOfDataRows) {

        // std::set<size_t> legitSeeds = {967,15176,22391,52065,39383,18225,12447,31390,40465,47784,40340,15703,19242,14347,4836,37208,79014,14475,87634,19087,58383,76009,57137,63423,2174,18481,21931,16646,45138,76570,1913,51178,25044,55769,38681,16066,52066,4050,755,29183,44826,21319,82844,43125,42783,87356,31165,53853,3682,35102,45942,52984,113108,17547,15329,18619,41745,142777,17952,44867,7841,41019,46693,37298,7739,19526,15140,62537,14641,38952,19243,18981,37264,29986,45240,15178,8107,67405,40731,48251,14537,26968,3738,77258,63558,24777,27134,60702,26947,123945,3714,55996,75479,38222,12090,7627,32497,1319,63776,39023};
        // for (auto e: legitSeeds)
        //     randomOrder.push_back(e);
        for (size_t i = 0; i < numberOfDataRows; i++) {
            // if (legitSeeds.find(i) == legitSeeds.end())
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