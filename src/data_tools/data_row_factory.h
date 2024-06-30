#include <iostream>
#include <sstream>
#include <fstream>
#include <memory>
#include <map>
#include <optional>

class LineFactory {
    public:
    virtual std::optional<std::string> maybeGet() = 0;
};

class FromFileLineFactory : public LineFactory {
    private:
    std::istream &source;

    public:
    FromFileLineFactory(std::istream &source) : source(source) {}

    std::optional<std::string> maybeGet() {
        std::string data;

        if (!std::getline(source, data)) {
            return std::nullopt;
        }

        return move(data);
    }
};

class GeneratedLineFactory : public LineFactory {
};

class DataRowFactory {
    public:
    virtual DataRow* maybeGet(LineFactory &source) = 0;
    virtual std::unique_ptr<DataRow> getFromNaiveBinary(std::vector<double> binary) const = 0;
    virtual std::unique_ptr<DataRow> getFromBinary(std::vector<double> binary) const = 0;
};

// TODO: This should be a static constructor in the dense data row? 
class DenseDataRowFactory : public DataRowFactory {
    public:
    DataRow* maybeGet(LineFactory &source) {
        std::optional<std::string> data(source.maybeGet());
        if (!data.has_value()) {
            return nullptr;
        }
        
        std::vector<double> result;

        char *token;
        char *rest = data.value().data();

        while ((token = strtok_r(rest, DELIMETER.data(), &rest)))
            result.push_back(std::stod(std::string(token)));

        return new DenseDataRow(move(result));
    }

    std::unique_ptr<DataRow> getFromNaiveBinary(std::vector<double> binary) const {
        return std::unique_ptr<DataRow>(new DenseDataRow(move(binary)));
    }

    std::unique_ptr<DataRow> getFromBinary(std::vector<double> binary) const {
        return this->getFromNaiveBinary(move(binary));
    }
};

// TODO: This should be a static constructor in the sparse data row? 
class SparseDataRowFactory : public DataRowFactory {
    private:
    const static size_t EXPECTED_ELEMENTS_PER_LINE = 3;

    const size_t totalColumns;

    long expectedRow;
    long currentRow;
    double value = 1.0; 
    long to;
    bool hasData;

    public:
    SparseDataRowFactory(const size_t totalColumns) : 
        expectedRow(0), 
        hasData(false),
        totalColumns(totalColumns) 
    {}

    DataRow* maybeGet(LineFactory &source) {
        std::map<size_t, double> result;

        while (true) {
            if (this->hasData) {
                if (this->currentRow == this->expectedRow) {
                    result.insert({to, value});
                } else {
                    this->expectedRow++;
                    return new SparseDataRow(move(result), this->totalColumns);
                }
            }

            std::optional<std::string> line(source.maybeGet());
            if (!line.has_value()) {
                if (this->hasData) {
                    this->hasData = false;
                    return new SparseDataRow(move(result), this->totalColumns);
                } else {
                    return nullptr;
                }
            }

            this->hasData = false;

            std::istringstream stream(line.value().data());
            double number;
            size_t totalSeen = 0;

            // The expected format is a two ints a line, and 
            //  maybe a third reprsenting value.
            while(stream >> number && totalSeen < EXPECTED_ELEMENTS_PER_LINE) {
                switch (totalSeen) {
                    case 0:
                        currentRow = std::floor(number);
                    case 1:
                        to = std::floor(number);
                    case 2:
                        value = number;
                }

                if (stream.peek() == ',')
                    stream.ignore();

                totalSeen++;
            }
            
            if (currentRow == this->expectedRow) {
                result.insert({to, value});
            } else {
                this->expectedRow++;
                this->hasData = true;
                return new SparseDataRow(move(result), this->totalColumns);
            }
        } 
    }

    std::unique_ptr<DataRow> getFromNaiveBinary(std::vector<double> binary) const {
        std::map<size_t, double> res;
        for (size_t i = 0; i < binary.size(); i++) {
            if (binary[i] != 0) {
                res.insert({static_cast<size_t>(i), binary[i]});
            }
        }

        return std::unique_ptr<DataRow>(new SparseDataRow(move(res), this->totalColumns));    
    }

    std::unique_ptr<DataRow> getFromBinary(std::vector<double> binary) const {
        std::map<size_t, double> res;

        if (binary.size() % 2 != 0) {
            throw std::invalid_argument("ERROR: Received an input that is not divisible by two. This will likely cause a seg fault");
        }

        // Notice that this is +2 for every iteration, not plus 1
        for (size_t i = 0; i < binary.size(); i += 2) {
            res.insert({static_cast<size_t>(binary[i]), binary[i + 1]});
        }

        return std::unique_ptr<DataRow>(new SparseDataRow(move(res), this->totalColumns));

    }
};