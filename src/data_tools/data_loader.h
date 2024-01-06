#include <vector>
#include <string>
#include <iterator>
#include <iostream>
#include <sstream>
#include <fstream>
#include <stdio.h>
#include <cassert>
#include <string.h>
#include <optional>
#include <fmt/core.h>

static std::string DELIMETER = ",";

class DataLoader {
    public:
    virtual bool getNext(std::vector<double> &result) = 0;
};

class AsciiDataLoader : public DataLoader {
    private:
    std::istream &source;

    public:
    AsciiDataLoader(std::istream &input) : source(input) {}

    bool getNext(std::vector<double> &result) {
        std::string data;
        if (!std::getline(this->source, data)) {
            return false;
        }
        
        result.clear();

        char *token;
        char *rest = data.data();

        while ((token = strtok_r(rest, DELIMETER.data(), &rest)))
            result.push_back(std::stod(std::string(token)));

        return true;
    }
};

class BinaryDataLoader : public DataLoader {
    private:
    std::istream &source;
    std::optional<unsigned int> vectorSize;

    public:
    BinaryDataLoader(std::istream &input) : source(input) {
        this->vectorSize = std::nullopt;
    }

    bool getNext(std::vector<double> &result) {
        if (this->source.peek() == EOF) {
            return false;
        }

        result.clear();

        if (!this->vectorSize.has_value()) {
            unsigned int totalData;
            this->source.read(reinterpret_cast<char *>(&totalData), sizeof(totalData));
            this->vectorSize = totalData;
        }

        result.resize(this->vectorSize.value());
        this->source.read(reinterpret_cast<char *>(result.data()), this->vectorSize.value() * sizeof(double));
        return true;

    }

};