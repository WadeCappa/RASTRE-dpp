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

class DataFormater {
    public:
    virtual void buildElement(std::string &input, std::vector<double> &result) const = 0; 
    virtual std::string elementToString(const std::vector<double> &element) const = 0;
};

class AsciiDataFormater : public DataFormater {
    public:
    void buildElement(std::string &input, std::vector<double> &result) const {
        result.clear();
        char *token;
        char *rest = input.data();

        while ((token = strtok_r(rest, DELIMETER.data(), &rest)))
            result.push_back(std::stod(std::string(token)));
    }

    std::string elementToString(const std::vector<double> &element) const {
        std::string output = "";
        for (const auto & v : element) {
            output += std::to_string(v) + DELIMETER;
        }
        output.pop_back();
        return output;
    }
};

class BinaryDataFormater : public DataFormater {
    public:
    void buildElement(std::string &input, std::vector<double> &result) const {
        result.clear();
        std::istringstream stream(input);

        unsigned int totalData;
        stream.read(reinterpret_cast<char *>(&totalData), sizeof(totalData));
        result.resize(totalData);
        stream.read(reinterpret_cast<char *>(result.data()), totalData * sizeof(double));
    }

    std::string elementToString(const std::vector<double> &element) const {
        std::stringstream stream;
        
        unsigned int size = element.size();
        stream.write(reinterpret_cast<const char *>(&size), sizeof(size));
        stream.write(reinterpret_cast<const char *>(element.data()), size * sizeof(double));
        
        return stream.str();
    }
};