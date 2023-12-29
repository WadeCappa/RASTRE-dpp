#include <vector>
#include <string>
#include <iterator>
#include <iostream>
#include <sstream>
#include <fstream>
#include <stdio.h>
#include <cassert>
#include <string.h>

static std::string DELIMETER = ",";

class DataLoader {
    public:
    virtual bool loadNext() = 0;
    virtual std::vector<double> returnLoaded() = 0;

    // Visible for testing only
    static std::vector<double> buildElement(std::string &input) {
        std::vector<double> element;
        char *token;
        char *rest = input.data();

        while ((token = strtok_r(rest, DELIMETER.data(), &rest)))
            element.push_back(std::stod(std::string(token)));

        return element;
    }
};

class AsciiDataLoader : public DataLoader {
    private:
    bool reachedEndOfFile;
    std::vector<double> data;
    std::istream &source;

    public:
    AsciiDataLoader(std::istream &input) : reachedEndOfFile(false), source(input) {}

    // Should return true when there is more data to read, otherwise false.
    bool loadNext() {
        assert(!this->reachedEndOfFile);
        std::string nextLine;

        if (std::getline(source, nextLine)) {
            this->data = buildElement(nextLine);
            return true;
        } else {
            this->reachedEndOfFile = true;
            return false;
        }
    }
    
    std::vector<double> returnLoaded() {
        return this->data;
    }
};