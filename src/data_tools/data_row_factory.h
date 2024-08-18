#include <iostream>
#include <sstream>
#include <fstream>
#include <memory>
#include <map>
#include <optional>

#include <random>

class RandomNumberGenerator {
    public:
    virtual double getNumber() = 0;
    virtual void skipNextElements(size_t elementsToSkip) = 0;
};

class AlwaysOneGenerator : public RandomNumberGenerator {
    public:
    static std::unique_ptr<RandomNumberGenerator> create() {
        return std::unique_ptr<RandomNumberGenerator>(new AlwaysOneGenerator());
    }
    
    void skipNextElements(size_t _elementsToSkip) {
        // no-op
    }

    double getNumber() {
        return 1;
    }
};

class NormalRandomNumberGenerator : public RandomNumberGenerator {
    private:
    std::default_random_engine eng;
    std::normal_distribution<double> distribution;

    public:
    NormalRandomNumberGenerator(
        std::default_random_engine eng, 
        std::normal_distribution<double> distribution
    ) : eng(move(eng)), distribution(move(distribution)) {}

    static std::unique_ptr<RandomNumberGenerator> create(const long unsigned int seed) {
        std::default_random_engine eng(seed);
        std::normal_distribution<double> distribution;
        return std::unique_ptr<RandomNumberGenerator>(new NormalRandomNumberGenerator(move(eng), move(distribution)));
    }
    
    void skipNextElements(size_t elementsToSkip) {
        eng.discard(elementsToSkip);
    }

    double getNumber() {
        return distribution(eng);
    }
};

class UniformRandomNumberGenerator : public RandomNumberGenerator {
    private:
    std::default_random_engine eng;
    std::uniform_real_distribution<double> distribution;

    public:
    UniformRandomNumberGenerator(
        std::default_random_engine eng, 
        std::uniform_real_distribution<double> distribution
    ) : eng(move(eng)), distribution(move(distribution)) {}

    static std::unique_ptr<RandomNumberGenerator> create(const long unsigned int seed) {
        std::default_random_engine eng(seed);
        std::uniform_real_distribution<double> distribution(0.0, 1.0);
        return std::unique_ptr<RandomNumberGenerator>(new UniformRandomNumberGenerator(move(eng), move(distribution)));
    }
    
    void skipNextElements(size_t elementsToSkip) {
        eng.discard(elementsToSkip);
    }

    double getNumber() {
        return distribution(eng);
    }

};

class LineFactory {
    public:
    virtual std::optional<std::string> maybeGet() = 0;
    virtual void skipNext() = 0;
};

class FromFileLineFactory : public LineFactory {
    private:
    std::istream &source;

    public:
    FromFileLineFactory(std::istream &source) : source(source) {}

    void skipNext() {
        std::string data;
        std::getline(source, data);
    }

    std::optional<std::string> maybeGet() {
        std::string data;

        if (!std::getline(source, data)) {
            return std::nullopt;
        }

        return move(data);
    }
};

class GeneratedDenseLineFactory : public LineFactory {
    private:
    static constexpr const char* delimeter = ",";

    const size_t numRows;
    const size_t numColumns;
    std::unique_ptr<RandomNumberGenerator> rng;

    size_t currentRow;

    public:
    GeneratedDenseLineFactory(
        const size_t numRows,
        const size_t numColumns,
        std::unique_ptr<RandomNumberGenerator> rng
    ) : 
        numRows(numRows),
        numColumns(numColumns),
        rng(move(rng)),
        currentRow(0)
    {}

    void skipNext() {
        this->rng->skipNextElements(this->numColumns);
    }

    std::optional<std::string> maybeGet() {
        if (this->currentRow >= this->numRows) {
            return std::nullopt;
        }
        
        std::stringstream res;

        for (size_t i = 0; i < this->numColumns - 1; i++) {
            res << this->rng->getNumber() << ",";
        }
        res << this->rng->getNumber();

        this->currentRow++;
        return res.str();
    }
};

class GeneratedSparseLineFactory : public LineFactory {
    private:
    const size_t numRows;
    const size_t numColumns;
    const double sparsity;
    std::unique_ptr<RandomNumberGenerator> edgeValueRng;
    std::unique_ptr<RandomNumberGenerator> includeEdgeRng;

    size_t currentRow;
    size_t currentColumn;

    public:
    GeneratedSparseLineFactory(
        const size_t numRows,
        const size_t numColumns,
        const double sparsity,
        std::unique_ptr<RandomNumberGenerator> edgeValueRng,
        std::unique_ptr<RandomNumberGenerator> includeEdgeRng 
    ) : 
        numRows(numRows),
        numColumns(numColumns),
        sparsity(sparsity),
        edgeValueRng(move(edgeValueRng)),
        includeEdgeRng(move(includeEdgeRng)),
        currentRow(0),
        currentColumn(0)
    {}

    void skipNext() {
        this->includeEdgeRng->skipNextElements(this->numColumns);
        this->edgeValueRng->skipNextElements(this->numColumns);
    }

    std::optional<std::string> maybeGet() {
        if (this->currentRow >= this->numRows) {
            return std::nullopt;
        }

        double edgeValue = this->edgeValueRng->getNumber();

        while (this->includeEdgeRng->getNumber() < this->sparsity) {
            this->currentColumn++;

            // generate another value to make sure skips by row are accurate.
            edgeValue = this->edgeValueRng->getNumber();
        }

        if (this->currentColumn >= this->numColumns) {
            this->currentRow++;
            this->currentColumn %= this->numColumns;
        }

        std::stringstream res;

        // should use a constant to denote the delimeter here, pull from the 
        //  file that loads sparse rows
        std::string delimeter = " ";
        res << this->currentRow << delimeter << this->currentColumn << delimeter << edgeValue;
        
        // increment column to avoid repeats
        this->currentColumn++;

        return res.str();
    }
};

class DataRowFactory {
    public:
    virtual std::unique_ptr<DataRow> maybeGet(LineFactory &source) = 0;
    virtual std::unique_ptr<DataRow> getFromNaiveBinary(std::vector<double> binary) const = 0;
    virtual std::unique_ptr<DataRow> getFromBinary(std::vector<double> binary) const = 0;
    virtual void skipNext(LineFactory &source) = 0;
};

class DenseDataRowFactory : public DataRowFactory {
    public:
    std::unique_ptr<DataRow> maybeGet(LineFactory &source) {
        std::optional<std::string> data(source.maybeGet());
        if (!data.has_value()) {
            return nullptr;
        }
        
        std::vector<double> result;

        char *token;
        char *rest = data.value().data();

        while ((token = strtok_r(rest, DELIMETER.data(), &rest)))
            result.push_back(std::stod(std::string(token)));
        return std::unique_ptr<DataRow>(new DenseDataRow(move(result)));
    }

    void skipNext(LineFactory &source) {
        source.skipNext();
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

    void skipNext(LineFactory &source) {
        std::unique_ptr<DataRow> _line(maybeGet(source));
    }

    std::unique_ptr<DataRow> maybeGet(LineFactory &source) {
        std::map<size_t, double> result;

        while (true) {
            if (this->hasData) {
                if (this->currentRow == this->expectedRow) {
                    result.insert({to, value});
                } else {
                    this->expectedRow++;
                    return std::unique_ptr<DataRow>(new SparseDataRow(move(result), this->totalColumns));
                }
            }

            std::optional<std::string> line(source.maybeGet());
            if (!line.has_value()) {
                if (this->hasData) {
                    this->hasData = false;
                    return std::unique_ptr<DataRow>(new SparseDataRow(move(result), this->totalColumns));
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
                return std::unique_ptr<DataRow>(new SparseDataRow(move(result), this->totalColumns));
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