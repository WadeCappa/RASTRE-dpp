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
#include <limits.h>
#include <fmt/core.h>

static const std::string DELIMETER = ",";

class DataLoader {
    public:
    virtual bool getNext(std::vector<double> &result) = 0;
};

class BlockDataLoader : public DataLoader {
    private:
    const size_t end;
    size_t count;
    std::istream &source;

    public:
    BlockDataLoader(std::istream &source, size_t start, size_t end) : end(end), count(0), source(source) {
        for (this->count = 0; this->count < start; this->count++) {
            if (this->attemptGetNextLine()) {
                break;
            }
        }
    }

    bool attemptGetNextLine() {
        if (this->incrementCountTestIfEndOfBlock()) {
            return false;
        }

        std::string data;
        bool success = static_cast<bool>(std::getline(this->source, data));
        return success;
    }

    bool incrementCountTestIfEndOfBlock() {
        bool endOfBlock = this->count >= this->end;
        this->count++;
        return endOfBlock;
    }
};

class AsciiDataLoader : public BlockDataLoader {
    private:
    std::istream &input;

    bool attemptGetNextLine(std::string &data) {
        if (this->incrementCountTestIfEndOfBlock()) {
            return false;
        }

        bool success = static_cast<bool>(std::getline(this->input, data));
        return success;
    }

    public:
    AsciiDataLoader(std::istream &source, size_t start = 0, size_t end = INT_MAX) 
    : BlockDataLoader(input, start, end), input(input) {}

    bool getNext(std::vector<double> &result) {
        std::string data;
        if (!this->attemptGetNextLine(data)) {
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

class BinaryDataLoader : public BlockDataLoader {
    private:
    std::istream &source;
    size_t rowSize;

    bool attemptGetNextLine(std::vector<double> &result) {
        if (this->incrementCountTestIfEndOfBlock()) {
            return false;
        }

        result.clear();
        result.resize(this->rowSize);
        this->source.read(reinterpret_cast<char *>(result.data()), this->rowSize * sizeof(double));
        return true;
    }


    static size_t getRowSize(std::istream &input) {
        unsigned int rowSize;
        input.read(reinterpret_cast<char *>(&rowSize), sizeof(rowSize));
        return rowSize;
    }

    public:
    BinaryDataLoader(std::istream &source, size_t start = 0, size_t end = INT_MAX) 
    : rowSize(getRowSize(source)), source(source), BlockDataLoader(source, start, end) {}

    bool getNext(std::vector<double> &result) {
        if (this->source.peek() == EOF) {
            return false;
        }

        return this->attemptGetNextLine(result);
    }
};