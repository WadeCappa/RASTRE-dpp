#include <vector>
#include <nlohmann/json.hpp>

#ifndef REPRESENTATIVE_SUBSET_H
#define REPRESENTATIVE_SUBSET_H

class Subset {
    public:
    virtual ~Subset() {}

    virtual float getScore() const = 0;
    virtual size_t getRow(const size_t index) const = 0;
    virtual size_t size() const = 0;
    virtual nlohmann::json toJson() const = 0;
    virtual const size_t* begin() const = 0;
    virtual const size_t* end() const = 0;

    static std::unique_ptr<Subset> of(const std::vector<size_t> &rows, const float score);
    static std::unique_ptr<Subset> ofCopy(const std::vector<size_t> rows, const float score);
    static std::unique_ptr<Subset> empty();
};

class MutableSubset : public Subset {
    public:
    virtual void addRow(const size_t row, const float marginalGain) = 0;
    virtual void finalize() = 0;

    static std::unique_ptr<Subset> upcast(std::unique_ptr<MutableSubset> mutableSubset) {
        mutableSubset->finalize();
        return std::unique_ptr<Subset>(move(mutableSubset));
    }
};

class NaiveMutableSubset : public MutableSubset {
    private:
    float score;
    std::vector<size_t> rows;

    public:
    NaiveMutableSubset() : score(0), rows(std::vector<size_t>()) {}
    NaiveMutableSubset(const std::vector<size_t> &rows, const float score) : rows(rows), score(score) {}

    void addRow(const size_t row, const float marginalGain) {
        this->rows.push_back(row);
        this->score += marginalGain;
    }

    float getScore() const {
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

    void finalize() {}

    static std::unique_ptr<MutableSubset> makeNew() {
        return std::unique_ptr<MutableSubset>(dynamic_cast<MutableSubset*>(new NaiveMutableSubset()));
    }
};

std::unique_ptr<Subset> Subset::ofCopy(
    const std::vector<size_t> rows, 
    const float score
) {
    return std::unique_ptr<Subset>(new NaiveMutableSubset(move(rows), score));
}

std::unique_ptr<Subset> Subset::of(
    const std::vector<size_t> &rows, 
    const float score
) {
    Subset* subset = dynamic_cast<Subset*>(new NaiveMutableSubset(rows, score));
    return std::unique_ptr<Subset>(subset);
}

std::unique_ptr<Subset> Subset::empty() {
    std::vector<size_t> emptyRows;
    return Subset::of(emptyRows, 0);
}

#endif