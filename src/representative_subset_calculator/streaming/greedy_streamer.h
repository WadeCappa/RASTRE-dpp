
class GreedyStreamer {
    public:
    virtual std::unique_ptr<Subset> resolveStream() = 0;
};

class SeiveGreedyStreamer : public GreedyStreamer {
    private:
    Receiver &receiver;
    CandidateConsumer &consumer;

    public:
    SeiveGreedyStreamer(
        Receiver &receiver, 
        CandidateConsumer &consumer
    ) : 
        receiver(receiver),
        consumer(consumer)
    {}
    
    std::unique_ptr<Subset> resolveStream() {
        bool stillReceiving = true;
        while (stillReceiving) {
            std::unique_ptr<CandidateSeed> nextSeed(receiver.receiveNextSeed(stillReceiving));
            consumer.accept(move(nextSeed));
        }

        std::cout << "getting best consumer, destroying in process" << std::endl;
        return consumer.getBestSolutionDestroyConsumer();
    }
};