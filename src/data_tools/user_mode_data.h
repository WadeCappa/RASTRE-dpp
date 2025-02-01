

class UserModeDataDecorator : public BaseData {
    private:
    const BaseData &delegate;
    const UserData &userData;
    const std::unordered_map<size_t, size_t> globalRowToLocalRow;

    public:
    static std::unique_ptr<UserModeDataDecorator> create(
        const BaseData &delegate, const UserData &userData
    ) {
        std::unordered_map<size_t, size_t> globalRowToLocalRow;
        const std::vector<unsigned long long> & cu(userData.getCu());
        for (size_t i = 0; i < cu.size(); i++) {
            globalRowToLocalRow.insert({cu[i], i});
        }

        return std::unique_ptr<UserModeDataDecorator>(
            new UserModeDataDecorator(delegate, userData, move(globalRowToLocalRow))
        );
    }

    /**
     * TODO: This might be out of range if we parallelize a user accross many machines. Will
     * need to fix.
     */
    const DataRow& getRow(size_t i) const {
        return this->delegate.getRow(this->userData.getCu()[i]);
    }

    size_t totalRows() const {
        return this->userData.getCu().size();
    }

    size_t totalColumns() const {
        return this->delegate.totalColumns();
    }

    size_t getRemoteIndexForRow(const size_t localRowIndex) const {
        return this->userData.getCu()[localRowIndex];
    }

    size_t getLocalIndexFromGlobalIndex(const size_t globalIndex) const {
        return globalRowToLocalRow.at(globalIndex);
    }

    private:
    UserModeDataDecorator(
        const BaseData &delegate, const UserData &userData, std::unordered_map<size_t, size_t> globalRowToLocalRow
    ) : delegate(delegate), userData(userData), globalRowToLocalRow(globalRowToLocalRow) {}
};
