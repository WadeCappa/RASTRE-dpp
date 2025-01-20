#include <unordered_map>
#include <format>

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

/**
 * TODO: needs to be threadsafe
 */
class MemoizedRelevanceCalculator : public RelevanceCalculator {
    private:
    std::unique_ptr<RelevanceCalculator> delegate;
    std::unordered_map<std::string, float> memo;

    static std::string get_key(const size_t i, const size_t j) {
        return std::to_string(i) + "," + std::to_string(j);
    }

    public:
    MemoizedRelevanceCalculator(std::unique_ptr<RelevanceCalculator> delegate) 
    : delegate(move(delegate)) {}

    float get(const size_t i, const size_t j) {
        std::string key = get_key(i, j);
        if (memo.find(key) != memo.end()) {
            return memo.at(key);
        }

        memo[key] = delegate->get(i, j);
        return memo.at(key);
    }
};

class UserModeRelevanceCalculator : public RelevanceCalculator {
    private:
    std::unique_ptr<RelevanceCalculator> delegate;
    const std::vector<double> &ru;
    const double alpha;

    public:
    /**
     * This constructor gets spammed. We should really cache this or otherwise make this more efficent
     */
    static std::unique_ptr<UserModeRelevanceCalculator> from(
        const BaseData &data, 
        const UserData &userData, 
        const double theta
    ) {
        const double alpha = calcAlpha(theta);
        SPDLOG_TRACE("calculated alpha of {0:f}", alpha);
        return std::unique_ptr<UserModeRelevanceCalculator>(
            new UserModeRelevanceCalculator(
                NaiveRelevanceCalculator::from(data), 
                userData.getRu(), 
                alpha
            )
        );
    }

    float get(const size_t i, const size_t j) {
        const double s_ij = this->delegate->get(i, j);
        const double r_i = getRu(i);
        const double r_j = getRu(j);
        const double result = r_i * s_ij * r_j;
        SPDLOG_TRACE("result for user mode of {0:f} from {1:d} of value {2:f} and {3:d} of value {4:f}", result, i, r_i, j, r_j);
        return result;
    }

    private:
    UserModeRelevanceCalculator(
        std::unique_ptr<RelevanceCalculator> delegate, 
        const std::vector<double> &ru,
        const double alpha
    ) : delegate(move(delegate)), ru(ru), alpha(alpha) {}

    double getRu(size_t i) const {
        return std::exp(this->alpha * this->ru[i]);
    }

    static double calcAlpha(const double theta) {
        return 0.5 * (theta / (1.0 - theta));
    }
};