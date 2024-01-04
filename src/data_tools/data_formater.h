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
            output += std::to_string(v) + ",";
        }
        output.pop_back();
        return output;
    }
};

class BinaryDataFormater : public DataFormater {
    public:
    void buildElement(std::string &input, std::vector<double> &result) const {
    }

    std::string elementToString(const std::vector<double> &element) const {
    }
};