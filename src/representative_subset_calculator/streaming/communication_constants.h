
#ifndef COMMUNICATION_CONSTANTS_H
#define COMMUNICATION_CONSTANTS_H

class CommunicationConstants {
    public:
    static unsigned int getStopTag() {
        return 1;
    }

    static unsigned int getContinueTag() {
        return 0;
    }

    static float endOfSendTag() {
        return -1;
    }
};

#endif