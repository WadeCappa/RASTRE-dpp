

class SendRequest {
    public:
    virtual bool sendCompleted() const = 0;
    virtual bool sendStarted() const = 0;
    virtual bool waitForCompletion() = 0; 

    virtual void sendAsync(const unsigned int tag) = 0;
    virtual void sendAndBlock(const unsigned int tag) = 0;
};