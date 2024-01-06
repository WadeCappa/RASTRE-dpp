#include <vector>
#include <string>
#include <iterator>
#include <iostream>
#include <sstream>
#include <fstream>
#include <stdio.h>
#include <cassert>
#include <string.h>

#include "data_formater.h"

class DataLoader {
    public:
    virtual bool getNext(std::vector<double> &result) = 0;
};

class IncrementalDataLoader : public DataLoader {
    private:
    bool reachedEndOfFile;
    std::istream &source;
    DataReader &reader;

    public:
    IncrementalDataLoader(DataReader &reader, std::istream &input) : reader(reader), reachedEndOfFile(false), source(input) {}

    bool getNext(std::vector<double> &result) {
        if (this->reachedEndOfFile) {
            return false;
        }

        this->reachedEndOfFile = !this->reader.read(result, source);
        return !this->reachedEndOfFile;
    }
};

class InstantDataLoader : public DataLoader { 
    private:
    std::istringstream data;
    DataReader &reader;
    bool reachedEndOfData;

    public:
    InstantDataLoader(DataReader &reader, std::istream &input) : reader(reader), reachedEndOfData(false) {
        std::ostringstream dataStream;
        dataStream << input.rdbuf();
        data = std::istringstream(dataStream.str());
    }

    bool getNext(std::vector<double> &result) { 
        if (reachedEndOfData) {
            return false;
        }

        this->reachedEndOfData = !this->reader.read(result, data);
        return !this->reachedEndOfData;
    }
};