#include "../../data_tools/data_row.h"

#ifndef CANDIDATE_SEED_H
#define CANDIDATE_SEED_H

class CandidateSeed {
    private:
    std::unique_ptr<DataRow> data;
    unsigned int globalRow;
    unsigned int originRank;

    public:
    CandidateSeed(
        const unsigned int row, 
        std::unique_ptr<DataRow> data,
        const unsigned int rank
    ) : 
        data(move(data)), 
        globalRow(row),
        originRank(rank)
    {}

    const DataRow &getData() const {
        return *(this->data.get());
    }

    unsigned int getRow() const {
        return this->globalRow;
    }

    unsigned int getOriginRank() const {
        return this->originRank;
    }
};

#endif