class UserScore {
    public:
    static double calculateMRR(const UserData& userData, const Subset& subset) {

        // start at one since the highest score we can get is 1 / 1
        size_t count = 1;
        for (const size_t row : subset) {
            if (row == userData.getTestId()) {
                return 1.0 / count;
            }

            count++;
        }

        return 0.0;
    }

    static double calculateILAD(const UserData& userData, RelevanceCalculator& calc) {
        double aggregate_scores;
        const std::vector<double> &ru = userData.getRu();
        #pragma omp parallel for reduction(+:aggregate_scores)
        for (size_t j = 0; j < ru.size(); j++) {
            for (size_t i = 0; i < ru.size(); i++) {
                if (j == i) {
                    continue;
                }

                aggregate_scores += 1.0 - calc.get(i, j);
            }
        }

        return aggregate_scores / (double)(ru.size() * 2.0);
    }

    static double calculateILMD(const UserData& userData, RelevanceCalculator& calc) {
        double minimum_score = std::numeric_limits<double>::max();
        const std::vector<double> &ru = userData.getRu();
        for (size_t j = 0; j < ru.size(); j++) {
            for (size_t i = 0; i < ru.size(); i++) {
                if (j == i) {
                    continue;
                }

                minimum_score = std::min(1.0 - calc.get(i, j), minimum_score);
            }
        }

        return minimum_score;
    }
};