#include <Eigen/Dense>
#include <vector>
#include <math.h>
#include <unordered_set>
#include <unordered_map>
#include <optional>

class LazyFastSubsetCalculator : public SubsetCalculator {
    private:
    const float epsilon;

    struct HeapComparitor {
        const std::vector<float> &diagonals;
        HeapComparitor(const std::vector<float> &diagonals) : diagonals(diagonals) {}
        bool operator()(size_t a, size_t b) {
            return std::log(diagonals[a]) < std::log(diagonals[b]);
        }
    };

    static std::vector<float> getSlice(
        const std::unordered_map<size_t, float> &row, 
        const MutableSubset* subset, 
        size_t count
    ) {
        std::vector<float> res(count);
        for (size_t i = 0; i < count ; i++) {
            res[i] = row.at(subset->getRow(i));
        }

        return res;
    }

    public:
    LazyFastSubsetCalculator(const float epsilon) : epsilon(epsilon) {
        if (this->epsilon < 0) {
            throw std::invalid_argument("Epsilon is less than 0.");
        }
    }

    std::unique_ptr<Subset> getApproximationSet(
        std::unique_ptr<MutableSubset> consumer, 
        std::unique_ptr<RelevanceCalculator> calc,
        const BaseData &data, 
        size_t k
    ) {
        
        std::unordered_set<size_t> seen;
        std::vector<std::unordered_map<size_t, float>> v(data.totalRows());
        std::vector<size_t> u(data.totalRows(), 0);
        
        // Just needs to pass diag(e^(alpha * r_u)) for our per-user calc. Should be an opt 
        // for the non-user case. For use during all kernel matrix opts
        std::unique_ptr<LazyKernelMatrix> kernelMatrix(LazyKernelMatrix::from(data, move(calc)));
        spdlog::debug("created lazy fast kernel matrix");
        
        // Account for user mode here
        std::vector<float> diagonals = kernelMatrix->getDiagonals();
        spdlog::debug("got diagonals for lazy fast kernel");
        
        // Initialize priority queue
        std::vector<size_t> priorityQueue;
        for (size_t index = 0; index < data.totalRows(); index++) {
            priorityQueue.push_back(index);
        }

        HeapComparitor comparitor(diagonals);
        std::make_heap(priorityQueue.begin(), priorityQueue.end(), comparitor);
        
        while (consumer->size() < k) {
            size_t i = priorityQueue.front();
            std::pop_heap(priorityQueue.begin(),priorityQueue.end(), comparitor); 
            priorityQueue.pop_back();
            
            // update row
            for (size_t t = u[i]; t < consumer->size(); t++) {

                size_t j_t = consumer->getRow(t); 
                float dotProduct = KernelMatrix::getDotProduct(this->getSlice(v[i], consumer.get(), t), this->getSlice(v[j_t], consumer.get(), t));                
                // account for user mode in ->get()
                float newScore = (kernelMatrix->get(i, j_t) - dotProduct) / std::sqrt(diagonals[j_t]);
                v[i].insert({j_t, newScore});                
                diagonals[i] -= std::pow(v[i][j_t], 2);
            }
            
            u[i] = consumer->size();
            
            // To ensure "Numerical stability" ¯\_(ツ)_/¯
            if (diagonals[i] < this->epsilon) {
                spdlog::info("breaking to ensure Numerical stability");
                break;
            }
            
            float marginalGain = std::log(diagonals[i]);
            float nextScore = std::log(diagonals[priorityQueue.front()]);

            if (marginalGain >= nextScore || consumer->size() == data.totalRows() - 1) {
                consumer->addRow(i, marginalGain);
            } else {
                priorityQueue.push_back(i);
                std::push_heap(priorityQueue.begin(), priorityQueue.end(), comparitor);
            }
        }
        return MutableSubset::upcast(move(consumer));
    }
};