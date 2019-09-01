#include <sqlite3.h>
#include <string>
#include <stdexcept>
#include <iostream>
#include <memory>

#ifndef UNTITLED_VEXSQL_HPP
#define UNTITLED_VEXSQL_HPP

inline void sqlite3_cpp_error(char* &err) {
    if(err) {
        std::string errStr(err);
        sqlite3_free(err);
        err = nullptr;
        throw std::runtime_error(errStr);
    }
}

struct ScopedSQLite3 {
    sqlite3* ptr;
    std::string location;
    std::string seqTableName;
    explicit ScopedSQLite3(const std::string& location): ptr(nullptr), location(location), seqTableName("event_sequence") {
        sqlite3_open(location.c_str(), &ptr);
    }
    ~ScopedSQLite3() {
        if(ptr) {
            sqlite3_close(ptr);
            std::cerr << "Database named " << location << " successfully closed." << std::endl;
        }
    }
    ScopedSQLite3(const ScopedSQLite3&) = delete;
    ScopedSQLite3& operator=(const ScopedSQLite3&) = delete;
    ScopedSQLite3(ScopedSQLite3&& other) noexcept {
        ptr = other.ptr;
        other.ptr = nullptr;
    }
    ScopedSQLite3& operator=(ScopedSQLite3&& other) noexcept {
        ptr = other.ptr;
        other.ptr = nullptr;
        return *this;
    }

    void executeScript(const std::string& data);

    template<typename T>
    void exec_lambda(const char* query, const T& copyableRunnable) {
        const T* lamb = &copyableRunnable;
        char* err = nullptr;
        sqlite3_exec(ptr, query, [](void* param, int colCount, char** colValues, char** colNames) -> int {
            (*((T*)param))(colCount, colValues, colNames);
        }, (void*)lamb, &err);
        sqlite3_cpp_error(err);
    }

    void exec(const char* query);

    int getQueryWithIntResult(const char* query);

    bool dbHasVexData() {
        return getQueryWithIntResult("SELECT count(*) FROM sqlite_master WHERE type='table' AND name='events';");
    }

    int getLastModified() {
        return dbHasVexData() ? getQueryWithIntResult(("SELECT max(modified) FROM " + seqTableName).c_str()) : 0;
    }

    int getSchema() {
        return getQueryWithIntResult("PRAGMA user_version");
    }

    bool applyEndpoint(const std::string& endpoint, int timeout = 0);
};

bool updateEventList(int timeout = 0);
ScopedSQLite3& dbForEventList(bool update = true);

inline ScopedSQLite3 dbForEndpoint(const std::string& endpoint) {
    ScopedSQLite3 db(":memory:");
    db.applyEndpoint(endpoint);
    return std::move(db);
}

#endif //UNTITLED_VEXSQL_HPP
