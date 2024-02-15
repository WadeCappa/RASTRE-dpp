#include "bucket.h"

class BucketController 
{
    private:
    std::vector<ThresholdBucket*> buckets;
    std::vector<std::vector<ThresholdBucket*>> threadMap;

    int k;
    double epsilon;
    
    size_t deltaZero;    
    int fullBuckets = 0;

    public:
    void CreateBuckets(size_t deltaZero)
    {
        this->deltaZero = deltaZero;

        int num_buckets = (int)(0.5 + [](double val, double base) {
            return log2(val) / log2(base);
        }((double)k, (1+this->epsilon)));

        // std::cout << "deltazero: " << this->deltaZero << ", number of buckets: " << num_buckets << std::endl;

        for (int i = 0; i < num_buckets + 1; i++)
        {
            this->buckets.push_back(new ThresholdBucket(this->deltaZero, this->k, this->epsilon, i));
        }
    }

    void MapBucketsToThreads(const unsigned int threads)
    {
        // build mapping of each thread to number of buckets
        int buckets_per_thread = std::ceil((double)this->buckets.size() / ((double)threads));
        // std::cout << "number of buckets; " << this->buckets.size() << ", threads; " << threads << ", buckets per thread; " << buckets_per_thread << std::endl;
        this->threadMap.resize(threads);

        int bucket_index = 0;
        for (int i = 0; i < this->threadMap.size(); i++ )
        {
            int buckets_added = 0;
            while (bucket_index < this->buckets.size() && buckets_added < buckets_per_thread)
            {
                this->threadMap[i].push_back(this->buckets.at(bucket_index));
                bucket_index++;
                buckets_added++;
            }
        }
    }

    void ProcessElements(
        const unsigned int threads, 
        // vector of size (#senders * kprime): each element is a pair -- first to indicate whether it has been received, second says the source and which seed it is
        const std::vector<std::pair<int, Origin>> &availability_index,
        // vector of size (#senders * kprime): each element is a pair -- first is the seed(row) ID, second is the vector representing the row in the dataset
        const std::vector<std::pair<size_t, std::vector<double>>> &element_matrix
    )
    {
        #pragma omp parallel for num_threads(threads)
        for (int i = 0; i < this->threadMap.size(); i++)
        {
            const std::vector<ThresholdBucket*>& thread_buckets = this->threadMap[i];
            int local_dummy_value = 0;

            for (int local_received_index = 0; local_received_index < availability_index.size(); local_received_index++)
            {
                while (true) 
                {
                    if (availability_index[local_received_index].first == 1)
                    {
                        break;
                    }

                    # pragma omp atomic
                    local_dummy_value++;
                }

                // std::cout << "inserting seed " << local_received_index << " " << availability_index[local_received_index].first << std::endl;

                for (const auto & b : thread_buckets)
                {
                    const auto & p = availability_index[local_received_index].second;
                    b->attemptInsert(element_matrix[p.source * p.seed]);
                }
            }
        }
    }

    size_t GetNumberOfBuckets()
    {
        return this->buckets.size();
    }

    std::vector<ThresholdBucket*> GetBuckets()
    {
        return this->buckets;
    }

    bool Initialized()
    {
        return this->buckets.size() > 0;
    }

    bool AllBucketsFull()
    {
        // return this->buckets.size() == 0 ? false : this->fullBuckets == this->buckets.size();
        return false;
    }

    std::pair<std::vector<unsigned int>, size_t> GetBestSeeds()
    {
        size_t max_utility = 0;
        int max_utility_index = 0;

        for (int i = 0; i < this->buckets.size(); i++)
        {
            size_t bucket_utility = this->buckets.at(i)->getUtility();
            if (bucket_utility > max_utility) {
                max_utility = bucket_utility;
                max_utility_index = i;
            }
        }
        
        std::vector<unsigned int> seeds;
        for (const auto p : this->buckets.at(max_covered_index)->getSolution()) {
            seeds.push_back(p.first);
        }

        // std::cout << "selected bucket " << max_covered_index << " with size of " << this->buckets.at(max_covered_index)->getSeeds().size() << std::endl;

        return std::make_pair(seeds, max_covered);
    }

    BucketController(int k, double epsilon)
    {
        this->k = k;
        this->epsilon = epsilon;
    }

    ~BucketController() 
    {
        for (auto b : this->buckets)
        {
            delete b;
        }
    }
};

