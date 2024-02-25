

class RankBuffer {
    public:
    virtual CandidateSeed* askForData() = 0;
    virtual bool stillReceiving() = 0;
};