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

class DataWriter {
    public:
    virtual void write(const std::vector<double> &element, std::ostream &out) = 0;
};

class DataReader {
    public:
    virtual bool read(std::vector<double> &result, std::istream &input) = 0; 
};

class AsciiDataWriter : public DataWriter {
    public:
    void write(const std::vector<double> &element, std::ostream &out) {
        std::ostringstream outputStream;
        for (const auto & v : element) {
            outputStream << fmt::format("{}", v) << DELIMETER;
        }
        std::string output = outputStream.str();
        output.pop_back();
        out << output << std::endl;
    }
};

class AsciiDataReader : public DataReader {
    public:
    bool read(std::vector<double> &result, std::istream &input) {
        std::string data;
        if (!std::getline(input, data)) {
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

class BinaryDataWriter : public DataWriter {
    private:
    std::optional<unsigned int> vectorSize;

    public:
    BinaryDataWriter() {
        this->vectorSize = std::nullopt;
    }

    void write(const std::vector<double> &element, std::ostream &out) {
        if (!this->vectorSize.has_value()) {
            this->vectorSize = element.size();
            unsigned int size = this->vectorSize.value();
            out.write(reinterpret_cast<const char *>(&size), sizeof(size));
        }
        
        out.write(reinterpret_cast<const char *>(element.data()), this->vectorSize.value() * sizeof(double));
    }
};

class BinaryDataReader : public DataReader {
    private:
    std::optional<unsigned int> vectorSize;

    public:
    BinaryDataReader() {
        this->vectorSize = std::nullopt;
    }

    bool read(std::vector<double> &result, std::istream &input) {
        if (input.peek() == EOF) {
            return false;
        }

        result.clear();

        if (!this->vectorSize.has_value()) {
            unsigned int totalData;
            input.read(reinterpret_cast<char *>(&totalData), sizeof(totalData));
            this->vectorSize = totalData;
        }

        result.resize(this->vectorSize.value());
        input.read(reinterpret_cast<char *>(result.data()), this->vectorSize.value() * sizeof(double));
        return true;
    }
};