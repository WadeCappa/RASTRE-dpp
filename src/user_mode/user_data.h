#include <fstream>
#include <sstream> 

class UserData {
    public:
    /**
     * The id of the user under test
     */
    virtual unsigned long long getUserId() const = 0;

    /**
     * This is the id of a control row that we _should_ see in the 
     * users recommended subset. If we do not see this row in the 
     * resulting subset, our calculation was wrong
     */
    virtual unsigned long long getTestId() const = 0;
    
    /**
     * This is the ground set for this user. We should only examine 
     * these row ids to calculate our subset
     */
    virtual const std::vector<unsigned long long> & getCu() const = 0;
    
    /**
     * DOUBLE CHECK THIS -> this is our user's recommendation weights.
     * This makes the user recommendations more accurate and needs to be
     * used while calculating scores while finding the reccomendation subset.
     */
    virtual const std::vector<double> & getRu() const = 0;

    /**
     * We sometimes might need the exact mapping of cu to ru in a lookup table 
     * instead of two arrays.
     */
    virtual const std::unordered_map<unsigned long long, double> & getCuToRuMapping() const = 0;
};

class UserDataImplementation : public UserData {
    private:
    const unsigned long long uid, tid;
    const std::vector<unsigned long long> cu;
    const std::vector<double> ru;
    const std::unordered_map<unsigned long long, double> cuToRuMapping;

    public:
    static std::vector<std::unique_ptr<UserData>> load(const std::string path) {
        std::ifstream inputFile;
        inputFile.open(path);
        std::vector<std::unique_ptr<UserData>> result(UserDataImplementation::load(inputFile));
        inputFile.close();
        return move(result);
    }

    static std::vector<std::unique_ptr<UserData>> loadForMultiMachineMode(
        const std::string path, const std::vector<unsigned int>& rowToRank, unsigned int rank) {
        std::ifstream inputFile;
        inputFile.open(path);
        std::vector<std::unique_ptr<UserData>> result(
            UserDataImplementation::loadForMultiMachineMode(
                inputFile, rowToRank, rank
            )
        );
        inputFile.close();
        return move(result);
    }

    /**
     * For multi-machine mode
     */
    static std::vector<std::unique_ptr<UserData>> loadForMultiMachineMode(
        std::istream &input, const std::vector<unsigned int>& rowToRank, unsigned int rank) {

        std::vector<std::unique_ptr<UserData>> raw(UserDataImplementation::load(input));

        std::vector<std::unique_ptr<UserData>> filtered_users;
        for (size_t u = 0; u < raw.size(); u++) {
            std::vector<unsigned long long> filtered_cu;
            std::vector<double> filtered_ru;
            const std::vector<unsigned long long>& cu(raw[u]->getCu());
            const std::vector<double>& ru(raw[u]->getRu());
            for (size_t cu_i = 0; cu_i < cu.size(); cu_i++) {
                if (rowToRank[cu[cu_i]] != rank) {
                    continue;
                }

                filtered_cu.push_back(cu[cu_i]);
                filtered_ru.push_back(ru[cu_i]);
            }

            filtered_users.push_back(
                UserDataImplementation::from(
                    raw[u]->getUserId(), raw[u]->getTestId(), move(filtered_cu), move(filtered_ru)
                )
            );
        }

        return move(filtered_users);
    }

    /**
     * Visible for testing
     */
    static std::vector<std::unique_ptr<UserData>> load(std::istream &input) {
        std::vector<std::unique_ptr<UserData>> result;
        std::string data;

        while (std::getline(input, data)) {
            unsigned long long uid, tid, lcu;
            std::vector<unsigned long long> cu;
            std::vector<double> ru;

            std::istringstream iss(data);
            std::string num;
            // UID TID LCU CU1 CU2 ... CU_LCU RU1 ... RU_LRU
            size_t i = 0;
            while (std::getline(iss, num, ' ')) {
                if (i == 0) {
                    uid = std::stoull(num);
                } else if (i == 1) {
                    tid = std::stoull(num);
                } else if (i == 2) {
                    lcu = std::stoull(num);
                } else if (i <= lcu + 2) {
                    cu.push_back(std::stoull(num));
                } else if (i <= (lcu * 2) + 2) {
                    ru.push_back(std::stod(num));
                } else {
                    spdlog::error("did not expect to reach {0:d} with lcu of {1:d}", i, lcu);
                }
                i++;
            }
            if (cu.size() != ru.size()) {
                spdlog::error("cu and ru are not the same size :: ru size of {0:d} and cu size of {1:d}", ru.size(), cu.size());
            }
            result.push_back(UserDataImplementation::from(uid, tid, move(cu), move(ru)));
        }

        return result;
    }

    /**
     * Visible for testing
     */
    static std::unique_ptr<UserData> from(
        const unsigned long long uid, 
        const unsigned long long tid, 
        const std::vector<unsigned long long> cu, 
        const std::vector<double> ru
    ) {
        std::unordered_map<unsigned long long, double> cuToRuMapping;

        for (size_t i = 0; i < cu.size(); i++) {
            cuToRuMapping.insert({cu[i], ru[i]});
        }

        return std::unique_ptr<UserData>(
            new UserDataImplementation(uid, tid, move(cu), move(ru), move(cuToRuMapping))
        );
    }

    unsigned long long getUserId() const {
        return this->uid;
    }

    unsigned long long getTestId() const {
        return this->tid;
    }

    const std::vector<unsigned long long> & getCu() const {
        return this->cu;
    }

    const std::vector<double> & getRu() const {
        return this->ru;
    }

    const std::unordered_map<unsigned long long, double> & getCuToRuMapping() const {
        return this->cuToRuMapping;
    }

    private:
    UserDataImplementation(
        const unsigned long long uid, 
        const unsigned long long tid, 
        const std::vector<unsigned long long> cu, 
        const std::vector<double> ru,
        const std::unordered_map<unsigned long long, double> cuToRuMapping
    ) : 
        uid(uid), 
        tid(tid), 
        cu(move(cu)), 
        ru(move(ru)),
        cuToRuMapping(move(cuToRuMapping)) {}
};