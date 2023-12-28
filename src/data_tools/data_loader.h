#include <vector>
#include <string>
#include <iterator>
#include <iostream>
#include <sstream>
#include <fstream>
#include <stdio.h>
#include <string.h>

static std::string DELIMETER = ",";

class DataLoader {
    private:
    static std::vector<double> buildElement(std::string &input) {
        std::vector<double> element;
        char *token;
        char *rest = input.data();

        while ((token = strtok_r(rest, DELIMETER.data(), &rest)))
            element.push_back(std::stod(std::string(token)));

        return element;
    }

    public:
    static std::vector<std::vector<double>> loadData(std::istream &input) {
        std::string line; 
        std::vector<std::vector<double>> data;

        while (std::getline(input, line)) {
            data.push_back(buildElement(line));
        }

        return data;
    }

    static std::vector<std::vector<double>> loadBinaryData(std::istream &input) {
        return std::vector<std::vector<double>>();
    }
};