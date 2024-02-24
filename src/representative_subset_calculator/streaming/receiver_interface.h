
class Receiver {
    public:
    virtual std::unique_ptr<CandidateSeed> receiveNextSeed(bool &stillReceiving) = 0;
};