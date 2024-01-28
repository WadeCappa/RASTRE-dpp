#include <math.h>
#include <vector>
#include "data_loader.h"

class BlockedDataLoader : public DataLoader {
    private:
    DataLoader &base;
    const size_t end;
    size_t count;

    public:
    BlockedDataLoader(DataLoader &base, size_t start, size_t end) : base(base), end(end), count(0) {
        std::vector<double> unusedData;
        for (; this->count < start; this->count++) {
            bool hasData = this->base.getNext(unusedData);
            if (!hasData) {
                break;
            }
        }
    }

    bool getNext(std::vector<double> &result) {
        if (this->count >= this->end) {
            return false;
        }

        if (this->base.getNext(result)) {
            this->count++;
            return true;
        }

        return false;
    }
};