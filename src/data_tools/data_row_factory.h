#include <iostream>
#include <sstream>
#include <fstream>
#include <memory>
#include <map>
#include <optional>
#include <numeric>

#include <random>

class RandomNumberGenerator {
    public:
    virtual ~RandomNumberGenerator() {}
    virtual float getNumber() = 0;
    virtual void skipNextElements(size_t elementsToSkip) = 0;
    virtual std::unique_ptr<RandomNumberGenerator> copy() = 0;
};

class AlwaysOneGenerator : public RandomNumberGenerator {
    public:
    static std::unique_ptr<RandomNumberGenerator> create() {
        return std::unique_ptr<RandomNumberGenerator>(new AlwaysOneGenerator());
    }
    
    void skipNextElements(size_t _elementsToSkip) {
        // no-op
    }

    float getNumber() {
        return 1;
    }

    std::unique_ptr<RandomNumberGenerator> copy() {
        return create();
    }
};

class NormalRandomNumberGenerator : public RandomNumberGenerator {
    private:
    std::default_random_engine eng;
    std::normal_distribution<float> distribution;

    public:
    NormalRandomNumberGenerator(
        std::default_random_engine eng, 
        std::normal_distribution<float> distribution
    ) : eng(move(eng)), distribution(move(distribution)) {}

    static std::unique_ptr<RandomNumberGenerator> create(const long unsigned int seed) {
        std::default_random_engine eng(seed);
        std::normal_distribution<float> distribution;
        return std::unique_ptr<RandomNumberGenerator>(new NormalRandomNumberGenerator(move(eng), move(distribution)));
    }
    
    void skipNextElements(size_t elementsToSkip) {
        eng.discard(elementsToSkip);
    }

    float getNumber() {
        return distribution(eng);
    }

    std::unique_ptr<RandomNumberGenerator> copy() {
        return std::unique_ptr<RandomNumberGenerator>(new NormalRandomNumberGenerator(this->eng, this->distribution));
    }
};

class UniformRandomNumberGenerator : public RandomNumberGenerator {
    private:
    std::default_random_engine eng;
    std::uniform_real_distribution<float> distribution;

    public:
    UniformRandomNumberGenerator(
        std::default_random_engine eng, 
        std::uniform_real_distribution<float> distribution
    ) : eng(move(eng)), distribution(move(distribution)) {}

    static std::unique_ptr<RandomNumberGenerator> create(const long unsigned int seed) {
        std::default_random_engine eng(seed);
        std::uniform_real_distribution<float> distribution(0.0, 1.0);
        return std::unique_ptr<RandomNumberGenerator>(new UniformRandomNumberGenerator(move(eng), move(distribution)));
    }
    
    void skipNextElements(size_t elementsToSkip) {
        eng.discard(elementsToSkip);
    }

    float getNumber() {
        return distribution(eng);
    }

    std::unique_ptr<RandomNumberGenerator> copy() {
        return std::unique_ptr<RandomNumberGenerator>(new UniformRandomNumberGenerator(this->eng, this->distribution));
    }
};

class LineFactory {
    public:
    virtual ~LineFactory() {}
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

class GeneratedLineFactory : public LineFactory {
    public:
    virtual void jumpToLine(const size_t line) = 0;
    virtual std::unique_ptr<GeneratedLineFactory> copy() = 0;
};

class GeneratedDenseLineFactory : public GeneratedLineFactory {
    private:
    static constexpr const char* delimeter = ",";

    const size_t numRows;
    const size_t numColumns;
    std::unique_ptr<RandomNumberGenerator> rng;

    size_t currentRow;

    GeneratedDenseLineFactory(
        const size_t numRows,
        const size_t numColumns,
        std::unique_ptr<RandomNumberGenerator> rng,
        size_t startingRow
    ) : 
        numRows(numRows),
        numColumns(numColumns),
        rng(move(rng)),
        currentRow(startingRow)
    {}

    static std::unique_ptr<GeneratedDenseLineFactory> create(
        const size_t numRows,
        const size_t numColumns,
        std::unique_ptr<RandomNumberGenerator> rng,
        size_t startingRow) {
        return std::unique_ptr<GeneratedDenseLineFactory>(new GeneratedDenseLineFactory(numRows, numColumns, move(rng), startingRow));
    }

    public:
    static std::unique_ptr<GeneratedDenseLineFactory> create(
        const size_t numRows,
        const size_t numColumns,
        std::unique_ptr<RandomNumberGenerator> rng) {
        return create(numRows, numColumns, move(rng), 0);
    }
    
    void skipNext() {
        this->jumpToLine(this->numColumns + 1);
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

    void jumpToLine(const size_t line) {
        if (line <= this->currentRow) {
            return;
        }
        const size_t elementsToSkip = (line - this->currentRow) * this->numColumns;
        this->rng->skipNextElements(elementsToSkip);
        this->currentRow = line;
    }
    
    std::unique_ptr<GeneratedLineFactory> copy() {
        return create(this->numRows, this->numColumns, rng->copy(), this->currentRow);
    }
};

class GeneratedSparseLineFactory : public GeneratedLineFactory {
    private:
    const size_t numRows;
    const size_t numColumns;
    const float sparsity;
    std::unique_ptr<RandomNumberGenerator> edgeValueRng;
    std::unique_ptr<RandomNumberGenerator> includeEdgeRng;

    size_t currentRow;
    size_t currentColumn;

    GeneratedSparseLineFactory(
        const size_t numRows,
        const size_t numColumns,
        const float sparsity,
        std::unique_ptr<RandomNumberGenerator> edgeValueRng,
        std::unique_ptr<RandomNumberGenerator> includeEdgeRng,
        const size_t currentRow,
        const size_t currentColumn
    ) : 
        numRows(numRows),
        numColumns(numColumns),
        sparsity(sparsity),
        edgeValueRng(move(edgeValueRng)),
        includeEdgeRng(move(includeEdgeRng)),
        currentRow(currentRow),
        currentColumn(currentColumn)
    {}

    static std::unique_ptr<GeneratedSparseLineFactory> create(
        const size_t numRows,
        const size_t numColumns,
        const float sparsity,
        std::unique_ptr<RandomNumberGenerator> edgeValueRng,
        std::unique_ptr<RandomNumberGenerator> includeEdgeRng,
        const size_t currentRow,
        const size_t currentColumn) {
        return std::unique_ptr<GeneratedSparseLineFactory>(new GeneratedSparseLineFactory(
            numRows, numColumns, sparsity, move(edgeValueRng), move(includeEdgeRng), currentRow, currentColumn
        ));
    }

    public:
    static std::unique_ptr<GeneratedSparseLineFactory> create(
        const size_t numRows,
        const size_t numColumns,
        const float sparsity,
        std::unique_ptr<RandomNumberGenerator> edgeValueRng,
        std::unique_ptr<RandomNumberGenerator> includeEdgeRng) {
        return create(numRows, numColumns, sparsity, move(edgeValueRng), move(includeEdgeRng), 0, 0);
    }

    void skipNext() {
        this->jumpToLine(this->currentRow + 1);
    }

    std::optional<std::string> maybeGet() {
        if (this->currentRow >= this->numRows) {
            return std::nullopt;
        }

        float edgeValue = this->edgeValueRng->getNumber();

        while (this->includeEdgeRng->getNumber() < this->sparsity) {
            this->currentColumn++;

            // generate another value to make sure skips by row are accurate.
            edgeValue = this->edgeValueRng->getNumber();

            // Can do this in a post-loop while loop for speed
            if (this->currentColumn >= this->numColumns) {
                this->currentRow++;
                this->currentColumn %= this->numColumns;
            }
        }

        if (this->currentRow >= this->numRows) {
            return std::nullopt;
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

    void jumpToLine(const size_t line) {
        if (line <= this->currentRow) {
            return;
        }

        const size_t elementsToSkip = ((line - this->currentRow) * this->numColumns) - this->currentColumn;
        this->includeEdgeRng->skipNextElements(elementsToSkip);
        this->edgeValueRng->skipNextElements(elementsToSkip);
        this->currentRow = line;

        // Since we subtracted the current column from the count earlier we know that
        //  we are starting from the 0th column of this new row.
        this->currentColumn = 0;
    }

    std::unique_ptr<GeneratedLineFactory> copy() {
        return create(this->numRows, this->numColumns, this->sparsity, this->edgeValueRng->copy(), this->includeEdgeRng->copy(), this->currentRow, this->currentColumn);
    }
};

class DataRowFactory {
    public:
    virtual std::unique_ptr<DataRow> maybeGet(LineFactory &source) = 0;
    virtual std::unique_ptr<DataRow> getFromNaiveBinary(std::vector<float> binary) const = 0;
    virtual std::unique_ptr<DataRow> getFromBinary(std::vector<float> binary) const = 0;
    virtual void skipNext(LineFactory &source) = 0;
    virtual ~DataRowFactory() {}
    
    // Pretty horrific method to expose, but I need to expose this for 
    //  sparse generation since the sparse generator will always create
    //  a row for the currentRow+1 edge even when its supposed to be 
    //  skipped.
    virtual void jumpToLine(const size_t line) = 0;
    
    // Also pretty bad that I need this method
    virtual std::unique_ptr<DataRowFactory> copy() = 0;
};

class NormalizedDataRowFactory : public DataRowFactory {
    private:
    std::unique_ptr<DataRowFactory> delegate;

    class NormalizingDataRowVisitor : public ReturningDataRowVisitor<std::unique_ptr<DataRow>> {
        private:
        std::unique_ptr<DataRow> result;

        long double calcEuclidianNorm(const std::vector<float>& data) {
            return std::sqrt(
                std::inner_product(data.begin(), data.end(), data.begin(), 0.0L)
            );
        }

        public:
        NormalizingDataRowVisitor() : result(nullptr) {}

        std::unique_ptr<DataRow> get() {
            return move(result);
        }

        void visitDenseDataRow(const std::vector<float>& data) {
            const long double eclidian_norm = calcEuclidianNorm(data);

            std::vector<float> res;
            for (const float d : data) {
                res.push_back(d / eclidian_norm);
            }
            result = DenseDataRow::of(move(res));
        }

        void visitSparseDataRow(const std::map<size_t, float>& data, size_t totalColumns) {
            std::vector<float> values;
            for (const auto d : data) {
                values.push_back(d.second);
            }
            const long double eclidian_norm = calcEuclidianNorm(values);

            std::map<size_t, float> res;
            for (const auto d : data) {
                res.insert({d.first, d.second / eclidian_norm});
            }
            result = std::unique_ptr<SparseDataRow>(new SparseDataRow(move(res), totalColumns));
        }
    };

    public:
    NormalizedDataRowFactory(std::unique_ptr<DataRowFactory> delegate) 
    : delegate(move(delegate)) {}

    std::unique_ptr<DataRow> maybeGet(LineFactory &source) {
        std::unique_ptr<DataRow> base(delegate->maybeGet(source));
        if (base == nullptr) {
            return nullptr;
        }

        NormalizingDataRowVisitor visitor;
        return base->visit(visitor);
    }
    
    std::unique_ptr<DataRow> getFromNaiveBinary(std::vector<float> binary) const {
        return delegate->getFromNaiveBinary(binary);
    }

    std::unique_ptr<DataRow> getFromBinary(std::vector<float> binary) const {
        return delegate->getFromBinary(binary);
    }

    void skipNext(LineFactory &source) {
        delegate->skipNext(source);
    }

    void jumpToLine(const size_t line) {
        delegate->jumpToLine(line);
    }
    
    std::unique_ptr<DataRowFactory> copy() {
        std::unique_ptr<DataRowFactory> base(delegate->copy());
        return std::unique_ptr<DataRowFactory>(new NormalizedDataRowFactory(move(base)));
    }
};

class DenseDataRowFactory : public DataRowFactory {
    public:
    std::unique_ptr<DataRow> maybeGet(LineFactory &source) {
        std::optional<std::string> data(source.maybeGet());
        if (!data.has_value()) {
            return nullptr;
        }
        
        std::vector<float> result;

        char *token;
        char *rest = data.value().data();

        while ((token = strtok_r(rest, DELIMETER.data(), &rest)))
            result.push_back(std::stod(std::string(token)));
        return std::unique_ptr<DataRow>(new DenseDataRow(move(result)));
    }

    void skipNext(LineFactory &source) {
        source.skipNext();
    }

    std::unique_ptr<DataRow> getFromNaiveBinary(std::vector<float> binary) const {
        return std::unique_ptr<DataRow>(new DenseDataRow(move(binary)));
    }

    std::unique_ptr<DataRow> getFromBinary(std::vector<float> binary) const {
        return this->getFromNaiveBinary(move(binary));
    }

    void jumpToLine(const size_t _line) {
        // no-op, no state
    }

    std::unique_ptr<DataRowFactory> copy() {
        return std::unique_ptr<DataRowFactory>(new DenseDataRowFactory());
    }
};

/**
 * This is a stateful class, we cannot re-use this between loads.
 */
class SparseDataRowFactory : public DataRowFactory {
    private:
    const static size_t EXPECTED_ELEMENTS_PER_LINE = 3;

    const size_t totalColumns;

    long expectedRow;
    long currentRow;
    float value = 1.0; 
    long to;
    bool hasData;

    public:
    SparseDataRowFactory(const size_t totalColumns) : 
        expectedRow(0), 
        hasData(false),
        totalColumns(totalColumns) 
    {}

    void skipNext(LineFactory &source) {
        maybeGet(source, true);
    }

    std::unique_ptr<DataRow> maybeGet(LineFactory &source) {
        return maybeGet(source, false);
    }

    std::unique_ptr<DataRow> getFromNaiveBinary(std::vector<float> binary) const {
        std::map<size_t, float> res;
        for (size_t i = 0; i < binary.size(); i++) {
            if (binary[i] != 0) {
                res.insert({static_cast<size_t>(i), binary[i]});
            }
        }

        return std::unique_ptr<DataRow>(new SparseDataRow(move(res), this->totalColumns));    
    }

    std::unique_ptr<DataRow> getFromBinary(std::vector<float> binary) const {
        std::map<size_t, float> res;

        if (binary.size() % 2 != 0) {
            throw std::invalid_argument("ERROR: Received an input that is not divisible by two. This will likely cause a seg fault");
        }

        // Notice that this is +2 for every iteration, not plus 1
        for (size_t i = 0; i < binary.size(); i += 2) {
            res.insert({static_cast<size_t>(binary[i]), binary[i + 1]});
        }

        return std::unique_ptr<DataRow>(new SparseDataRow(move(res), this->totalColumns));
    }

    void jumpToLine(const size_t line) {
        if (line <= this->expectedRow) {
            return;
        }
        this->hasData = false;
        this->expectedRow = line;
    }

    std::unique_ptr<DataRowFactory> copy() {
        return std::unique_ptr<DataRowFactory>(new SparseDataRowFactory(this->totalColumns));
    }

    private:
    std::unique_ptr<DataRow> maybeGet(LineFactory &source, bool skip) {
        std::map<size_t, float> result;

        if (this->hasData) {
            if (this->currentRow == this->expectedRow) {
                result.insert({to, value});
            } else {
                this->expectedRow++;
                return this->returnResult(move(result), skip);
            }
        }

        while (true) {
            std::optional<std::string> line(source.maybeGet());
            if (!line.has_value()) {
                if (result.size() > 0) {
                    this->hasData = false;
                    return this->returnResult(move(result), skip);
                } else {
                    return nullptr;
                }
            }
            SPDLOG_TRACE("loaded line of {}", line.value());

            this->hasData = false;

            std::istringstream stream(line.value().data());
            float number;
            size_t totalSeen = 0;

            // The expected format is a two ints a line, and 
            //  maybe a third reprsenting value.
            while(stream >> number && totalSeen < EXPECTED_ELEMENTS_PER_LINE) {
                switch (totalSeen) {
                    case 0:
                        currentRow = std::floor(number);
                        break;
                    case 1:
                        to = std::floor(number);
                        break;
                    case 2:
                        value = number;
                        break;
                }

                if (stream.peek() == ',')
                    stream.ignore();

                totalSeen++;
            }
            
            if (currentRow == this->expectedRow) {
                result.insert({to, value});
            } else if (currentRow > this->expectedRow) {
                this->expectedRow++;
                this->hasData = true;
                return this->returnResult(move(result), skip);
            } else {
                spdlog::error("had current row of {0:d} and expected row of {1:d}", currentRow, this->expectedRow);
                throw std::invalid_argument("ERROR: cannot backtrack");
            }
        } 
    }

    std::unique_ptr<DataRow> returnResult(std::map<size_t, float> result, bool skip) {
        if (skip) {
            return nullptr;
        }

        for (size_t i = 0; i < this->totalColumns; i++) {
            const float v = result.find(i) == result.end() ? 0.0 : result.at(i);
            std::cout << v << " ";
        }
        std::cout << std::endl;

        return std::unique_ptr<DataRow>(new SparseDataRow(move(result), this->totalColumns));
    }
};