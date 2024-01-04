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
    virtual bool getNext(std::vector<double> &result) = 0;
};

class AsciiDataLoader : public DataLoader {
    private:
    bool reachedEndOfFile;
    std::istream &source;

    void buildElement(std::string &input, std::vector<double> &result) {
        result.clear();
        char *token;
        char *rest = input.data();

        while ((token = strtok_r(rest, DELIMETER.data(), &rest)))
            result.push_back(std::stod(std::string(token)));
    }

    public:
    AsciiDataLoader(std::istream &input) : reachedEndOfFile(false), source(input) {}

    bool getNext(std::vector<double> &result) {
        if (this->reachedEndOfFile) {
            return false;
        }

        std::string nextline;

        if (std::getline(source, nextline)) {
            buildElement(nextline, result);
            return true;
        } else {
            this->reachedEndOfFile = true;
            return false;
        }
    }
};
