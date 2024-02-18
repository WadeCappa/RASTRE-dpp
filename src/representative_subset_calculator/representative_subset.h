#include <vector>
#include <nlohmann/json.hpp>

class Subset {
    public:
    virtual double getScore() const = 0;
    virtual size_t getRow(const size_t index) const = 0;
    virtual size_t size() const = 0;
    virtual nlohmann::json toJson() const = 0;
    virtual const size_t* begin() const = 0;
    virtual const size_t* end() const = 0;

    static std::unique_ptr<Subset> of(const std::vector<size_t> &rows, const double score);
    static std::unique_ptr<Subset> empty();
};

class MutableSubset : public Subset {
    public:
    virtual void addRow(const size_t row, const double marginalGain) = 0;

    static std::unique_ptr<Subset> upcast(std::unique_ptr<MutableSubset> mutableSubset) {
        return std::unique_ptr<Subset>(move(mutableSubset));
    }
};

class NaiveMutableSubset : public MutableSubset {
    private:
    double score;
    std::vector<size_t> rows;

    public:
    NaiveMutableSubset() : score(0), rows(std::vector<size_t>()) {}
    NaiveMutableSubset(const std::vector<size_t> &rows, const double score) : rows(rows), score(score) {}

    void addRow(const size_t row, const double marginalGain) {
        this->rows.push_back(row);
        this->score += marginalGain;
    }

    double getScore() const {
        return score;
    }

    size_t getRow(const size_t index) const {
        return this->rows[index];
    }

    size_t size() const {
        return this->rows.size();
    }

    const size_t* begin() const {
        return this->rows.data();
    }

    const size_t* end() const {
        return this->rows.data() + this->rows.size();
    }

    nlohmann::json toJson() const {
        nlohmann::json output {
            {"rows", this->rows}, 
            {"totalCoverage", this->score}
        };

        return output;
    }

    static std::unique_ptr<MutableSubset> makeNew() {
        return std::unique_ptr<MutableSubset>(dynamic_cast<MutableSubset*>(new NaiveMutableSubset()));
    }
};

std::unique_ptr<Subset> Subset::of(
    const std::vector<size_t> &rows, 
    const double score
) {
    Subset* subset = dynamic_cast<Subset*>(new NaiveMutableSubset(rows, score));
    return std::unique_ptr<Subset>(subset);
}

std::unique_ptr<Subset> Subset::empty() {
    std::vector<size_t> emptyRows;
    return Subset::of(emptyRows, 0);
}