

class RankBuffer {
    public:
    virtual CandidateSeed* askForData() = 0;
    virtual bool stillReceiving() = 0;
    virtual unsigned int getRank() const = 0;
    virtual double getLocalSolutionScore() const = 0;
    virtual std::unique_ptr<Subset> getLocalSolutionDestroyBuffer() = 0;
};