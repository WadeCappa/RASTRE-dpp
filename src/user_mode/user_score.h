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

    static double calculateILAD(const UserData& userData, const SegmentedData& data, const RelevanceCalculator& calc) {
        double aggregate_scores;
        size_t total_scores = 0;
        const std::vector<double> &ru = userData.getRu();
        for (size_t j = 0; j < ru.size(); j++) {
            for (size_t i = 0; i < ru.size(); i++) {
                if (j == i) {
                    continue;
                }

                aggregate_scores += 1.0 - calc.get(i, j);
                total_scores++;
            }
        }

        return aggregate_scores / (double)total_scores;
    }

    static double calculateILMD(const UserData& userData, const SegmentedData& data, const RelevanceCalculator& calc) {
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