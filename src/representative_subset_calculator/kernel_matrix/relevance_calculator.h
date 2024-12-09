
class RelevanceCalculator {
    public:
    virtual ~RelevanceCalculator() {}
    virtual float get(const size_t i, const size_t j) const = 0;
};

class NaiveRelevanceCalculator : public RelevanceCalculator {
    private:
    const BaseData &data;

    public:
    NaiveRelevanceCalculator(const BaseData &data) : data(data) {}

    static std::unique_ptr<NaiveRelevanceCalculator> from(const BaseData &data) {
        return std::make_unique<NaiveRelevanceCalculator>(data);
    }

    float get(const size_t i, const size_t j) const {
        return this->data.getRow(i).dotProduct(this->data.getRow(j)) + static_cast<int>(i == j);
    }
};

class UserModeRelevanceCalculator : public RelevanceCalculator {
    private:
    const BaseData &data;
    const std::vector<double> &ru;
    const double alpha;

    public:
    static std::unique_ptr<UserModeRelevanceCalculator> from(
        const BaseData &data, 
        const UserData &userData, 
        const double theta
    ) {
        return std::make_unique<UserModeRelevanceCalculator>(data, userData.getRu(), calcAlpha(theta));
    }

    UserModeRelevanceCalculator(
        const BaseData &data, 
        const std::vector<double> &ru,
        const double alpha
    ) : data(data), ru(ru), alpha(alpha) {}

    float get(const size_t i, const size_t j) const {
        const double s_ij = this->data.getRow(i).dotProduct(this->data.getRow(j)) + static_cast<int>(i == j);
        const double r_i = getRu(i);
        const double r_j = getRu(j);
        return r_i * s_ij * r_j;
    }

    private:
    double getRu(size_t i) const {
        return std::exp(this->alpha * this->ru[i]);
    }

    static double calcAlpha(const double theta) {
        return 0.5 * (theta / (1.0 - theta));
    }
};