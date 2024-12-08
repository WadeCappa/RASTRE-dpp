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
     * 
     */
    virtual const std::vector<unsigned long long> & getPu() const = 0;
    
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
};

class UserDataImplementation : public UserData {
    private:
    const unsigned long long uid, tid;
    const std::vector<unsigned long long> pu, cu;
    const std::vector<double> ru;

    public:
    static std::vector<std::unique_ptr<UserData>> load(const AppData &appData) {
        std::ifstream inputFile;
        inputFile.open(appData.loadInput.inputFile);
        std::vector<std::unique_ptr<UserData>> result(UserDataImplementation::load(inputFile));
        inputFile.close();
        return move(result);
    }

    /**
     * Visible for testing
     */
    static std::vector<std::unique_ptr<UserData>> load(std::istream &input) {
        std::vector<std::unique_ptr<UserData>> result;
        std::string data;

        while (std::getline(input, data)) {
            unsigned long long uid, tid, lpu, lcu;
            std::vector<unsigned long long> cu, pu;
            std::vector<double> ru;

            std::istringstream iss(data);
            std::string num;
            // UID TID LPU PU1 PU2 ... PU_LPU LCU CU1 CU2 ... CU_LCU RU1 ... RU_LRU
            size_t i = 0;
            while (std::getline(iss, num, ' ')) {
                if (i == 0) {
                    uid = std::stoull(num);
                } else if (i == 1) {
                    tid = std::stoull(num);
                } else if (i == 2) {
                    lpu = std::stoull(num);
                } else if (i > 2 && i < lpu) {
                    pu.push_back(std::stoull(num));
                } else if (i == lpu) {
                    lcu = std::stoull(num);
                } else if (i > lpu && i < lcu) {
                    cu.push_back(std::stoull(num));
                } else if (i < (lcu * 2)) {
                    ru.push_back(std::stod(num));
                } else {
                    spdlog::error("did not expect to reach this");
                }
                i++;
            }

            result.push_back(std::unique_ptr<UserData>(new UserDataImplementation(uid, tid, move(pu), move(cu), move(ru))));
        }

        return result;
    }

    unsigned long long getUserId() const {
        return this->uid;
    }

    unsigned long long getTestId() const {
        return this->tid;
    }

    const std::vector<unsigned long long> & getPu() const {
        return this->pu;
    }

    const std::vector<unsigned long long> & getCu() const {
        return this->cu;
    }

    const std::vector<double> & getRu() const {
        return this->ru;
    }

    private:
    UserDataImplementation(
        const unsigned long long uid, 
        const unsigned long long tid, 
        const std::vector<unsigned long long> pu, 
        const std::vector<unsigned long long> cu, 
        const std::vector<double> ru
        ) : 
        uid(uid), 
        tid(tid), 
        pu(move(pu)), 
        cu(move(cu)), 
        ru(move(ru)) {}
};