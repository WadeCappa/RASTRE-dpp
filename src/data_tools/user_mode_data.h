

class UserModeDataDecorator : public BaseData {
    private:
    const BaseData &delegate;
    const UserData &userData;

    public:
    UserModeDataDecorator(const BaseData &delegate, const UserData &userData) 
    : delegate(delegate), userData(userData) {}

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
};
