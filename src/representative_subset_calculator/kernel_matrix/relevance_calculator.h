


class RelevanceCalculator {
    public:
    virtual double get(const size_t a, const size_t b) const = 0;
};

class NaiveRelevanceCalculator : public RelevanceCalculator {
    private:
    const BaseData &data;

    public:
    NaiveRelevanceCalculator(const BaseData &data) : data(data) {}

    double get(const size_t a, const size_t b) const {
        return this->data.getRow(a).dotProduct(this->data.getRow(b)) + static_cast<int>(a == b);
    }
};