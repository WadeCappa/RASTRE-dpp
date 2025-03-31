
#include "base_data.h"
#include "../user_mode/user_data.h"

#ifndef USER_MODE_DATA_H
#define USER_MODE_DATA_H

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
            new UserModeDataDecorator(delegate, userData, std::move(globalRowToLocalRow))
        );
    }

    const DataRow& getRow(size_t i) const {
        size_t globalRow = this->userData.getCu()[i];
        size_t localRow = this->delegate.getLocalIndexFromGlobalIndex(globalRow);
        return this->delegate.getRow(localRow);
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
    ) : delegate(delegate), userData(userData), globalRowToLocalRow(move(globalRowToLocalRow)) {}
};

#endif