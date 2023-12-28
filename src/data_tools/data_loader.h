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
    private:
    bool endOfFile;
    std::vector<double> data;
    std::istream &source;

    public:
    DataLoader(std::istream &input) : endOfFile(false), source(input) {}

    // Should return true when there is more data to read, otherwise false.
    bool next() {
        if (this->endOfFile) {
            return false;
        }

        std::string nextLine;

        if (std::getline(source, nextLine)) {
            this->data = buildElement(nextLine);
            return true;
        } else {
            this->endOfFile = true;
            return false;
        }
    }
    
    std::vector<double> getElement() {
        return this->data;
    }

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