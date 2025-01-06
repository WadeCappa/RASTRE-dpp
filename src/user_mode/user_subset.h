class UserSubset : public Subset {
    private:
    std::unique_ptr<Subset> delegate;
    const UserData &userData;
    const double MRR_score;
    const double ILAD_score;
    const double ILMD_score;

    public:
    UserSubset(
        std::unique_ptr<Subset> delegate, 
        const UserData &userData, 
        const double MRR_score,
        const double ILAD_score,
        const double ILMD_score)
    : delegate(move(delegate)), userData(userData), MRR_score(MRR_score), ILAD_score(ILAD_score), ILMD_score(ILMD_score) {}

    static std::unique_ptr<UserSubset> create(
        std::unique_ptr<Subset> delegate, 
        const UserData &userData,
        const RelevanceCalculator& calc) {
        
        const double mrr = UserScore::calculateMRR(userData, *delegate);
        const double ilad = UserScore::calculateILAD(userData, calc);
        const double ilmd = UserScore::calculateILMD(userData, calc);
        return std::unique_ptr<UserSubset>(new UserSubset(move(delegate), userData, mrr, ilad, ilmd));
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
            {"MRR", MRR_score}, 
            {"ILAD", ILAD_score}, 
            {"ILMD", ILMD_score}
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