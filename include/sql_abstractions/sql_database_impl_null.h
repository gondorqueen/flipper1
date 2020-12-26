#pragma once
#include "sql_abstractions/sql_database_impl_base.h"
namespace sql {
class DatabaseImplNull : public DatabaseImplBase{
    public:
    DatabaseImplNull():DatabaseImplBase(){}
    void setConnectionToken(ConnectionToken) override;
    bool open()override;
    bool isOpen() const override;
    bool transaction() override;
    bool commit() override;
    bool rollback() override;
    void close() override;
    std::string connectionName() override;
    void *internalPointer() override;
    bool isNull() override;
    std::string driverType() const override;
};

}
