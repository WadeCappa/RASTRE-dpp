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

    public:
    static std::vector<std::vector<double>> loadData(std::ifstream &input) {
        std::string line; 
        std::vector<std::vector<double>> data;

        while (std::getline(input, line)) {
            std::vector<double> element;
            char *token;
            char *rest = line.data();

            while ((token = strtok_r(rest, DELIMETER.data(), &rest)))
                element.push_back(std::stod(std::string(token)));

            data.push_back(element);
        }

        return data;
    }

    static std::vector<std::vector<double>> loadBinaryData(std::ifstream &input) {
        return std::vector<std::vector<double>>();
    }
};