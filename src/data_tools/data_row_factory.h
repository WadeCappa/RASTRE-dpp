#include <iostream>
#include <sstream>
#include <fstream>
#include <memory>
#include <map>

class DataRowFactory {
    public:
    virtual DataRow* maybeGet(std::istream &source) = 0;
    virtual std::unique_ptr<DataRow> getFromSentBinary(std::vector<double> binary) const = 0;
};

// TODO: This should be a static constructor in the dense data row? 
class DenseDataRowFactory : public DataRowFactory {
    public:
    DataRow* maybeGet(std::istream &source) {
        std::string data;
        if (!std::getline(source, data)) {
            return nullptr;
        }
        
        std::vector<double> result;

        char *token;
        char *rest = data.data();

        while ((token = strtok_r(rest, DELIMETER.data(), &rest)))
            result.push_back(std::stod(std::string(token)));

        return new DenseDataRow(move(result));
    }

    std::unique_ptr<DataRow> getFromSentBinary(std::vector<double> binary) const {
        return std::unique_ptr<DataRow>(new DenseDataRow(move(binary)));
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

    DataRow* maybeGet(std::istream &source) {
        std::string data;
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

            if (!std::getline(source, data)) {
                if (this->hasData) {
                    this->hasData = false;
                    return new SparseDataRow(move(result), this->totalColumns);
                } else {
                    return nullptr;
                }
            }

            this->hasData = false;

            std::istringstream stream(data);
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

    std::unique_ptr<DataRow> getFromSentBinary(std::vector<double> binary) const {
        std::map<size_t, double> res;

        for (
            size_t i = 0;
            i < binary.size();
            
            // Notice that this is +2 for every iteration, not plus 1
            i += 2
        ) {
            if (binary[i] == CommunicationConstants::getNoMoreEdgesTag()) {
                return std::unique_ptr<DataRow>(new SparseDataRow(move(res), this->totalColumns));
            }
            res.insert({binary[i], binary[i + 1]});
        }

        throw std::invalid_argument("failed to finish building buffer");
    }
};