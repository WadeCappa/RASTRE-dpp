#include <math.h>
#include <vector>
#include "data_loader.h"

class BlockedDataLoader : public DataLoader {
    private:
    DataLoader &base;
    size_t count;
    const unsigned int rank;
    const std::vector<unsigned int> &rowToRank;

    public:
    BlockedDataLoader(DataLoader &base, const std::vector<unsigned int> &rowToRank, unsigned int rank) : base(base), rowToRank(rowToRank), count(0), rank(rank) {}

    // TODO: Refactor this to make skipped elements much cheaper. Might have to reach into the 
    //  underlying classes themselves?
    bool getNext(std::vector<double> &result) {
        std::vector<double> badResult;
        while (this->rowToRank[this->count] != this->rank) {
            this->base.getNext(badResult);
            this->count++;
        }

        if (this->base.getNext(result)) {
            return true;
        }

        return false;
    }
};