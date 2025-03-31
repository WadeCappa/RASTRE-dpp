
#ifndef RANK_BUFFER_H
#define RANK_BUFFER_H

class RankBuffer {
    public:
    virtual ~RankBuffer() {}
    virtual CandidateSeed* askForData() = 0;
    virtual bool stillReceiving() = 0;
    virtual unsigned int getRank() const = 0;
    virtual float getLocalSolutionScore() const = 0;
    virtual std::unique_ptr<Subset> getLocalSolutionDestroyBuffer() = 0;
};

#endif