#include <vector>
#include <memory>

#ifndef BUCKET_H
#define BUCKET_H

class ThresholdBucket 
{
    private:
    std::unique_ptr<MutableSubset> solution;
    std::unique_ptr<std::vector<const DataRow *>> solutionRows; 
    std::unique_ptr<std::vector<float>> d; 
    std::unique_ptr<std::vector<std::unique_ptr<DenseDataRow>>> b; 

    const float threshold;
    const int k;

    public:
    ~ThresholdBucket() {}
    ThresholdBucket(const float threshold, const int k) 
    : 
        threshold(threshold), 
        k(k), 
        solution(NaiveMutableSubset::makeNew()), 
        solutionRows(std::make_unique<std::vector<const DataRow *>>()),
        d(std::make_unique<std::vector<float>>()),
        b(std::make_unique<std::vector<std::unique_ptr<DenseDataRow>>>())
    {}

    ThresholdBucket(
        const float threshold, 
        const int k, 
        std::unique_ptr<MutableSubset> nextSolution,
        std::unique_ptr<std::vector<const DataRow *>> solutionRows,
        std::unique_ptr<std::vector<float>> d,
        std::unique_ptr<std::vector<std::unique_ptr<DenseDataRow>>> b
    ) : 
        threshold(threshold), 
        k(k), 
        solution(std::move(nextSolution)),
        solutionRows(std::move(solutionRows)),
        d(std::move(d)),
        b(std::move(b))
    {}

    bool isFull() const {
        return this->solution->size() >= this->k;
    }

    std::unique_ptr<ThresholdBucket> transferContents(const float newThreshold) {
        return std::unique_ptr<ThresholdBucket>(
            new ThresholdBucket(
                newThreshold, 
                this->k, 
                std::move(this->solution), 
                std::move(this->solutionRows), 
                std::move(this->d), 
                std::move(this->b)
            )
        );
    }

    double getThreshold() {
        return this->threshold ;
    }

    size_t getUtility() {
        return this->solution->getScore();
    }

    std::unique_ptr<Subset> returnSolutionDestroyBucket() {
        return MutableSubset::upcast(std::move(this->solution));
    }

    bool attemptInsert(size_t rowIndex, const DataRow &data) {
        SPDLOG_TRACE("trying to insert seed {0:d} into bucket with threshold {1:f}", rowIndex, this->threshold);
        if (this->solution->size() >= this->k) {
            return false;
        }
        
        // TODO: Verify the correctness of the +1 here. This might not be right.
        float d_i = std::sqrt(data.dotProduct(data) + 1);
        std::unique_ptr<DenseDataRow> c_i(new DenseDataRow());

        for (size_t j = 0; j < this->solution->size(); j++) {
            if (!this->passesThreshold(std::log(std::pow(d_i, 2)))) {
                return false;
            }
            const float e_i = (data.dotProduct(*(solutionRows->at(j))) - c_i->dotProduct(*b->at(j))) / d->at(j);
            c_i->push_back(e_i);
            d_i = std::sqrt(std::pow(d_i, 2) - std::pow(e_i, 2));
        }
        
        const float marginal = std::log(std::pow(d_i, 2));
        
        if (this->passesThreshold(marginal)) {
            SPDLOG_TRACE("seed {0:d} with mirginal of {1:f} passed threshold of {2:f}", rowIndex, marginal, this->threshold);
            this->solution->addRow(rowIndex, marginal);
            this->solutionRows->push_back(&data);
            this->d->push_back(d_i);
            this->b->push_back(std::move(c_i));
            return true;
        } 

        return false;
    }

    private:
    bool passesThreshold(float marginalGain) {
        return marginalGain >= (((this->threshold / 2) - this->solution->getScore()) / (this->k - this->solution->size()));
    }
};

#endif