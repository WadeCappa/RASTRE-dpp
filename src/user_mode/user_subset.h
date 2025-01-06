class UserSubset : public Subset {
    private:
    std::unique_ptr<Subset> delegate;
    const UserData &userData;
    const SegmentedData& data; 
    const RelevanceCalculator& calc;

    public:
    UserSubset(
        std::unique_ptr<Subset> delegate, 
        const UserData &userData, 
        const SegmentedData& data, 
        const RelevanceCalculator& calc)
    : delegate(move(delegate)), userData(userData), data(data), calc(calc) {}

    static std::unique_ptr<UserSubset> create(
        std::unique_ptr<Subset> delegate, 
        const UserData &userData, 
        const SegmentedData& data, 
        const RelevanceCalculator& calc) {
        return std::unique_ptr<UserSubset>(new UserSubset(move(delegate), userData, data, calc));
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
            {"userId", userData.getUserId()}, 
            {"MRR", UserScore::calculateMRR(userData, *delegate)}, 
            {"ILAD", UserScore::calculateILAD(userData, data, calc)}, 
            {"ILMD", UserScore::calculateILMD(userData, data, calc)}
        };

        output.push_back(delegate->toJson());
        return output;
    }

    const size_t* begin() const {
        return delegate->begin();
    }

    const size_t* end() const {
        return delegate->end();
    }
};