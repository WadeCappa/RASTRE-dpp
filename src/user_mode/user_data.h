class UserData {
    public:
    virtual double getUserId() const = 0;
    virtual double getTestId() const = 0;
    virtual const std::vector<double> & getPu() const = 0;
    virtual const std::vector<double> & getCu() const = 0;
    virtual const std::vector<double> & getRu() const = 0;
};

class UserDataImplementation : public UserData {
    private:
    const double uid, tid;
    const std::vector<double> pu, cu, ru;

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
            double uid, tid, lpu, lcu;
            std::vector<double> cu, ru, pu;

            std::istringstream iss(data);
            std::string num;
            // UID TID LPU PU1 PU2 ... PU_LPU LCU CU1 CU2 ... CU_LCU RU1 ... RU_LRU
            double i = 0;
            while (std::getline(iss, num, ' ')) {
                if (i == 0) {
                    uid = std::stod(num);
                } else if (i == 1) {
                    tid = std::stod(num);
                } else if (i == 2) {
                    lpu = std::stod(num);
                } else if (i > 2 && i < lpu) {
                    pu.push_back(std::stod(num));
                } else if (i == lpu) {
                    lcu = std::stod(num);
                } else if (i > lpu && i < lcu) {
                    cu.push_back(std::stod(num));
                } else if (i < (lcu * 2)) {
                    ru.push_back(std::stod(num));
                } else {
                    spdlog::error("did not expect to reach this");
                }
            }

            result.push_back(std::unique_ptr<UserData>(new UserDataImplementation(uid, tid, move(cu), move(ru), move(pu))));
        }

        return result;
    }

    double getUserId() const {
        return this->uid;
    }

    double getTestId() const {
        return this->tid;
    }

    const std::vector<double> & getPu() const {
        return this->pu;
    }

    const std::vector<double> & getCu() const {
        return this->cu;
    }

    const std::vector<double> & getRu() const {
        return this->ru;
    }

    private:
    UserDataImplementation(
        const double uid, 
        const double tid, 
        const std::vector<double> pu, 
        const std::vector<double> cu, 
        const std::vector<double> ru
        ) : 
        uid(uid), 
        tid(tid), 
        pu(move(pu)), 
        cu(move(cu)), 
        ru(move(ru)) {}
};