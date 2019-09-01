#include "VEXSQL.hpp"
#include "ViaAPI.hpp"
#include <vector>

std::vector<std::string> split(std::string s, const std::string& delimiter) {
    std::vector<std::string> ret;
    size_t pos = 0;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        ret.push_back(s.substr(0, pos));
        s.erase(0, pos + delimiter.length());
    }
    if(!s.empty()) ret.push_back(s);
    return ret;
}

void ScopedSQLite3::executeScript(const std::string& data) {
    char* err = nullptr;
    for(auto& line: split(data, ";\n")) {
        if(line.empty()) continue;
        sqlite3_exec(ptr, line.c_str(), nullptr, nullptr, &err);
        sqlite3_cpp_error(err);
    }
}

void ScopedSQLite3::exec(const char* query) {
    char* err = nullptr;
    sqlite3_exec(ptr, query, nullptr, nullptr, &err);
    sqlite3_cpp_error(err);
}

bool ScopedSQLite3::applyEndpoint(const std::string &endpoint, int timeout) {
    std::string downloadedData;
    bool gotGoodResponse = false;
    do {
        std::cerr << "Downloading SQL data..." << std::endl;
        downloadedData = apiCall(endpoint, getLastModified(), getSchema(), timeout);
        if(!downloadedData.empty()) gotGoodResponse = true;
        std::cerr << "Executing SQL script..." << std::endl;
        executeScript(downloadedData);
    } while(!downloadedData.empty());
    return gotGoodResponse;
}

int ScopedSQLite3::getQueryWithIntResult(const char* query) {
    int ret = 0;
    exec_lambda(query, [&](int colCount, char** colValues, char** colNames) -> int {
        if(!colValues[0]) return 0;
        ret = (int)strtol(colValues[0], nullptr, 10);
        return 0;
    });
    return ret;
}

std::unique_ptr<ScopedSQLite3> eventListDB;

bool updateEventList(int timeout) {
    auto& db = dbForEventList();
    return db.applyEndpoint("events", timeout);
}

ScopedSQLite3& dbForEventList(bool update) {
    if(!eventListDB) {
        eventListDB = std::make_unique<ScopedSQLite3>("evt_list.db");
        eventListDB->exec("PRAGMA JOURNAL_MODE = OFF;");
        eventListDB->exec("PRAGMA SYNCHRONOUS = OFF;");
        eventListDB->exec("PRAGMA LOCKING_MODE = EXCLUSIVE;");
        eventListDB->seqTableName = "events_sequence";
        if(update) updateEventList();
    }
    return *eventListDB;
}