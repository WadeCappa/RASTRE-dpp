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
        const std::vector<size_t>& subset, 
        size_t count
    ) {
        std::vector<float> res(count);
        for (size_t i = 0; i < count ; i++) {
            res[i] = row.at(subset[i]);
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
        RelevanceCalculator& calc,
        const BaseData &data, 
        size_t k
    ) {
        
        std::unordered_set<size_t> seen;
        std::vector<std::unordered_map<size_t, float>> v(data.totalRows());
        std::vector<size_t> u(data.totalRows(), 0);
        
        // Just needs to pass diag(e^(alpha * r_u)) for our per-user calc. Should be an opt 
        // for the non-user case. For use during all kernel matrix opts
        std::unique_ptr<LazyKernelMatrix> kernelMatrix(LazyKernelMatrix::from(data, calc));
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

        spdlog::debug("built priority queue");

        std::vector<size_t> in_subset;

        while (consumer->size() < k) {
            const size_t i = priorityQueue.front();
            std::pop_heap(priorityQueue.begin(),priorityQueue.end(), comparitor); 
            priorityQueue.pop_back();
            
            // update row
            for (size_t t = u[i]; t < consumer->size(); t++) {
                const size_t j_t = in_subset[t]; 
                const float dotProduct = KernelMatrix::getDotProduct(
                    this->getSlice(v[i], in_subset, t), 
                    this->getSlice(v[j_t], in_subset, t)
                );
                // account for user mode in ->get()
                const float sqrt = std::sqrt(diagonals[j_t]);
                const float newScore = (kernelMatrix->get(i, j_t) - dotProduct) / sqrt;
                v[i].insert({j_t, newScore});                
                diagonals[i] -= std::pow(v[i][j_t], 2);
            }
            
            u[i] = consumer->size();
            
            if (diagonals[i] < this->epsilon) {
                spdlog::info("breaking to ensure Numerical stability");
                break;
            }
            
            const float marginalGain = std::log(diagonals[i]);
            const float nextScore = std::log(diagonals[priorityQueue.front()]);

            if (marginalGain >= nextScore || consumer->size() == data.totalRows() - 1) {
                consumer->addRow(i, marginalGain);
                in_subset.push_back(i);
            } else {
                priorityQueue.push_back(i);
                std::push_heap(priorityQueue.begin(), priorityQueue.end(), comparitor);
            }
        }
        return MutableSubset::upcast(move(consumer));
    }
};