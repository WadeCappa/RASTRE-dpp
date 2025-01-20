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
};