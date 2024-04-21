



class CommunicationConstants {
    public:
    static unsigned int getStopTag() {
        return 1;
    }

    static unsigned int getContinueTag() {
        return 0;
    }

    static double endOfSendTag() {
        return -1;
    }
};