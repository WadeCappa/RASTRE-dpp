
#ifndef USER_SUBSET_H
#define USER_SUBSET_H

class UserOutputInformationSubset : public Subset {
    private:
    std::unique_ptr<Subset> delegate;
    unsigned long long userId;
    unsigned long long testId;

    public:
    UserOutputInformationSubset(
        std::unique_ptr<Subset> delegate, 
        unsigned long long userId,
        unsigned long long testId 
    ) : delegate(move(delegate)), userId(userId), testId(testId) {}

    static std::unique_ptr<UserOutputInformationSubset> create(
        std::unique_ptr<Subset> delegate, 
        const UserData &userData) {
        
        return std::unique_ptr<UserOutputInformationSubset>(
            new UserOutputInformationSubset(
                move(delegate), 
                userData.getUserId(), 
                userData.getTestId()
            )
        );
    }

    static std::unique_ptr<UserOutputInformationSubset> translate(
        std::unique_ptr<Subset> delegate, 
        const UserData &userData) {

        std::vector<size_t> globalRows;
        for (const size_t r : *delegate) {
            globalRows.push_back(userData.getCu()[r]);
        }
        
        return std::unique_ptr<UserOutputInformationSubset>(
            new UserOutputInformationSubset(
                Subset::ofCopy(move(globalRows), delegate->getScore()), 
                userData.getUserId(), 
                userData.getTestId()
            )
        );
    }

    float getScore() const {
        return delegate->getScore();
    }

    size_t getRow(const size_t index) const {
        return delegate->getRow(index);
    }

    size_t size() const {
        return delegate->size();
    }

    nlohmann::json toJson() const {
        nlohmann::json output{
            {"userId", userId},
            {"testId", testId},
            {"solution", delegate->toJson()}
        };
        return output;
    }

    const size_t* begin() const {
        return delegate->begin();
    }

    const size_t* end() const {
        return delegate->end();
    }
};

#endif