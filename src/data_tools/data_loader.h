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

static const std::string DELIMETER = ",";

class DataLoader {
    public:
    virtual std::optional<DataRow*> getNext() = 0;
};

class AsciiAdjacencyListDataLoader : public DataLoader {
    private:
    const static size_t EXPECTED_ELEMENTS_PER_LINE = 3;

    const size_t totalColumns;

    long expectedRow;
    long currentRow;
    double value = 1.0; 
    long to;
    bool hasData;
    std::istream &source;

    public:
    AsciiAdjacencyListDataLoader(
        std::istream &input, 
        const size_t totalColumns
    ) : 
        source(input), 
        expectedRow(0), 
        hasData(false),
        totalColumns(totalColumns) 
    {}

    std::optional<DataRow*> getNext() {
        std::string data;
        std::vector<double> rawData(this->totalColumns, 0);

        while (true) {
            if (this->hasData) {
                if (this->currentRow == this->expectedRow) {
                    rawData[to] = value;
                } else {
                    this->expectedRow++;
                    return new NaiveDataRow(move(rawData));
                }
            }

            if (!std::getline(this->source, data)) {
                if (this->hasData) {
                    this->hasData = false;
                    return new NaiveDataRow(move(rawData));
                } else {
                    return std::nullopt;
                }
            }

            this->hasData = false;

            std::istringstream stream(data);
            long number;
            size_t totalSeen = 0;

            // The expected format is a two ints a line, and 
            //  maybe a third reprsenting value.
            while(stream >> number && totalSeen < EXPECTED_ELEMENTS_PER_LINE) {
                switch (totalSeen) {
                    case 0:
                        currentRow = number;
                    case 1:
                        to = number;
                    case 2:
                        value = number;
                }

                if (stream.peek() == ',')
                    stream.ignore();

                totalSeen++;
            }
            
            if (currentRow == this->expectedRow) {
                rawData[to] = value;
            } else {
                this->expectedRow++;
                this->hasData = true;
                return new NaiveDataRow(move(rawData));
            }
        } 
    }
};

class AsciiDataLoader : public DataLoader {
    private:
    std::istream &source;

    public:
    AsciiDataLoader(std::istream &input) : source(input) {}

    std::optional<DataRow*> getNext(DataRow **result) {
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
    BinaryDataLoader(std::istream &input) : source(input), vectorSize(std::nullopt) {}

    std::optional<DataRow*> getNext() {
        if (this->source.peek() == EOF) {
            return std::nullopt;
        }

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