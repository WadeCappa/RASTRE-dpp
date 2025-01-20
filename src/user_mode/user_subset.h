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

class TranslatingUserSubset : public MutableSubset {
    private:
    std::unique_ptr<MutableSubset> delegate;
    const UserModeDataDecorator &userDataDecorator;

    public:
    TranslatingUserSubset(
        std::unique_ptr<MutableSubset> delegate, 
        const UserModeDataDecorator &userDataDecorator
    ) : 
        delegate(move(delegate)), 
        userDataDecorator(userDataDecorator) {}

    static std::unique_ptr<TranslatingUserSubset> create(
        const UserModeDataDecorator &userDataDecorator) {
        
        return std::unique_ptr<TranslatingUserSubset>(
            new TranslatingUserSubset(NaiveMutableSubset::makeNew(), userDataDecorator)
        );
    }

    static std::unique_ptr<TranslatingUserSubset> create(
        std::unique_ptr<MutableSubset> delegate, 
        const UserModeDataDecorator &userDataDecorator) {
        
        return std::unique_ptr<TranslatingUserSubset>(
            new TranslatingUserSubset(
                move(delegate), 
                userDataDecorator
            )
        );
    }

    void addRow(const size_t row, const float marginalGain) {
        size_t global_row = userDataDecorator.getRemoteIndexForRow(row);
        delegate->addRow(global_row, marginalGain);
    }

    void finalize() {
        delegate->finalize();
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
        return delegate->toJson();
    }

    const size_t* begin() const {
        return delegate->begin();
    }

    const size_t* end() const {
        return delegate->end();
    }
};