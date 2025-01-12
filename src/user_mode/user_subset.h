class UserSubset : public Subset {
    private:
    std::unique_ptr<Subset> delegate;
    unsigned long long userId;

    public:
    UserSubset(
        std::unique_ptr<Subset> delegate, 
        unsigned long long userId
    ) : delegate(move(delegate)), userId(userId) {}

    static std::unique_ptr<UserSubset> create(
        std::unique_ptr<Subset> delegate, 
        const UserData &userData) {
        
        return std::unique_ptr<UserSubset>(new UserSubset(move(delegate), userData.getUserId()));
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