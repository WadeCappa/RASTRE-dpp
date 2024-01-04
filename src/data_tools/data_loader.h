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

class ConcreteDataLoader : public DataLoader {
    private:
    bool reachedEndOfFile;
    std::istream &source;
    const DataFormater &formater;

    public:
    ConcreteDataLoader(const DataFormater &formater, std::istream &input) : formater(formater), reachedEndOfFile(false), source(input) {}

    bool getNext(std::vector<double> &result) {
        if (this->reachedEndOfFile) {
            return false;
        }

        std::string nextline;

        if (std::getline(source, nextline)) {
            this->formater.buildElement(nextline, result);
            return true;
        } else {
            this->reachedEndOfFile = true;
            return false;
        }
    }
};
