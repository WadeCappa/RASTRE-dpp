
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
    std::unique_ptr<RelevanceCalculator> delegate;
    const std::vector<double> &ru;
    const double alpha;

    public:
    static std::unique_ptr<UserModeRelevanceCalculator> from(
        const BaseData &data, 
        const UserData &userData, 
        const double theta
    ) {
        const double alpha = calcAlpha(theta);
        spdlog::debug("user mode alpha of {0:f}", alpha);
        return std::make_unique<UserModeRelevanceCalculator>(
            NaiveRelevanceCalculator::from(data), 
            userData.getRu(), 
            alpha
        );
    }

    UserModeRelevanceCalculator(
        std::unique_ptr<RelevanceCalculator> delegate, 
        const std::vector<double> &ru,
        const double alpha
    ) : delegate(move(delegate)), ru(ru), alpha(alpha) {}

    float get(const size_t i, const size_t j) const {
        const double s_ij = this->delegate->get(i, j);
        const double r_i = getRu(i);
        const double r_j = getRu(j);
        const double result = r_i * s_ij * r_j;
        SPDLOG_TRACE("result for user mode of {0:f} from {1:d} of value {2:f} and {3:d} of value {4:f}", result, i, r_i, j, r_j);
        return result;
    }

    private:
    double getRu(size_t i) const {
        return std::exp(this->alpha * this->ru[i]);
    }

    static double calcAlpha(const double theta) {
        return 0.5 * (theta / (1.0 - theta));
    }
};