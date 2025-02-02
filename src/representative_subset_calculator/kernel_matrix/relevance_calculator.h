#include <unordered_map>

class RelevanceCalculator {
    public:
    virtual ~RelevanceCalculator() {}
    virtual float get(const size_t i, const size_t j) = 0;
};

class NaiveRelevanceCalculator : public RelevanceCalculator {
    private:
    const BaseData &data;

    public:
    NaiveRelevanceCalculator(const BaseData &data) : data(data) {}

    static std::unique_ptr<NaiveRelevanceCalculator> from(const BaseData &data) {
        return std::make_unique<NaiveRelevanceCalculator>(data);
    }

    float get(const size_t i, const size_t j) {
        return (1.0 + this->data.getRow(i).dotProduct(this->data.getRow(j))) / 2.0;
    }
};

class UserModeRelevanceCalculator : public RelevanceCalculator {
    private:
    std::unique_ptr<RelevanceCalculator> delegate;
    // not a reference
    const std::vector<double> ru;
    const double alpha;

    public:
    /**
     * This constructor gets spammed. We should really cache this or otherwise make this more efficent
     */
    static std::unique_ptr<UserModeRelevanceCalculator> from(
        const BaseData &data, 
        const std::vector<double> userData, 
        const double theta
    ) {
        const double alpha = calcAlpha(theta);
        SPDLOG_TRACE("calculated alpha of {0:f}", alpha);
        return std::unique_ptr<UserModeRelevanceCalculator>(
            new UserModeRelevanceCalculator(
                NaiveRelevanceCalculator::from(data), 
                move(userData), 
                alpha
            )
        );
    }

    float get(const size_t i, const size_t j) {
        const double s_ij = this->delegate->get(i, j);
        const double r_i = getRu(i);
        const double r_j = getRu(j);
        const double result = r_i * s_ij * r_j;
        SPDLOG_TRACE("result for user mode of {0:f} from s_ij of {1:f}, r_i {2:d} of value {3:f}, and r_j {4:d} of value {5:f}", result, s_ij, i, r_i, j, r_j);
        return result;
    }

    private:
    UserModeRelevanceCalculator(
        std::unique_ptr<RelevanceCalculator> delegate, 
        const std::vector<double> ru,
        const double alpha
    ) : delegate(move(delegate)), ru(move(ru)), alpha(alpha) {}

    double getRu(size_t i) const {
        return std::exp(this->alpha * this->ru[i]);
    }

    static double calcAlpha(const double theta) {
        return 0.5 * (theta / (1.0 - theta));
    }
};