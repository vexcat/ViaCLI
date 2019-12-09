#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <csignal>
#include "VEXSQL.hpp"
#include "json.hpp"
#include <condition_variable>

using namespace std::string_literals;
using json = nlohmann::json;

//Ctrl-C and SIGINT handler
//Flag before and after updating the database when doing --long-poll.
std::atomic<bool> shouldICrash(false);
std::condition_variable threadNotifier;
void shouldCrash(int) {
    shouldICrash.store(true);
    threadNotifier.notify_all();
}
void setIntHandler() {
    struct sigaction sa {};
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = shouldCrash;
    sigfillset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
}

json dumpQueryResults(ScopedSQLite3& db, const char* query) {
    json ret = json::array();
    db.exec_lambda(query, [&](int colCount, char** colValues, char** colNames) -> int {
        json obj = json::object();
        for(int i = 0; i < colCount; i++) {
            obj[colNames[i]] = colValues[i] ? colValues[i] : "(null)";
        }
        ret.push_back(obj);
        return 0;
    });
    return ret;
}

json dumpEntireDatabase(ScopedSQLite3& db) {
    json ret = json::object();
    db.exec_lambda("SELECT name FROM sqlite_master WHERE type = 'table';", [&](int colCount, char** colValues, char** colNames) -> int {
        ret[colValues[0]] = dumpQueryResults(db, ("SELECT * FROM "s + colValues[0]).c_str());
        return 0;
    });
    return ret;
}

void applyXform(json& arr, std::string key, std::string val) {
    if(!key.empty()) {
        json obj = json::object();
        for(auto& row: arr) (obj[row[key].get<std::string>()] = row).erase(key);
        arr = obj;
    }
    if(!val.empty()) {
        if(arr.is_object()) {
            auto& obj = arr;
            for(auto& [_, v]: obj.items()) {
                v = v[val];
            }
        } else {
            for(auto& v: arr) {
                v = v[val];
            }
        }
    }
}

void doCLI(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <list-events | list-events-no-network | detail-event <SKU>> <dump | do <SQL>> [--key column][--val column][--long-poll] [--timeout x]." << std::endl;
        std::cout << "list-events will give data on all events registered on VEX Via." << std::endl;
        std::cout << "detail-event-<SKU> will give data for particular event/SKU. (The end of a RobotEvents URL.)" << std::endl;
        std::cout << "Output is written to stdout in JSON." << std::endl;
        std::cout << "JSON will be indented if the --pretty option is specified." << std::endl;
        std::cout << "When running a query, an array will be returned. If you think a column of the table is suitable" << std::endl;
        std::cout << "  for assigning JSON keys and returning an object instead, use --key. --key sku is recommended" << std::endl;
        std::cout << "  when working with the events table. --val makes each array/object entry be a single column's" << std::endl;
        std::cout << "  data rather than the entire row, which can make things more readable in manual use.";
        std::cout << "The long-poll option will first work like normal, printing out the current state of VEX Via," << std::endl;
        std::cout << "    however the program will continue running and wait for VEX Via to change using a long-poll." << std::endl;
        std::cout << "    JSON Patches are used to print changes. Applications using long-polling should be flexible" << std::endl;
        std::cout << "    to both addition and retraction of data from VEX Via, even if it never actually happens," << std::endl;
        std::cout << "    so using a proper JSON-patching library is recommended. (see \"fast-json-patch\" on NPM)." << std::endl;
        std::cout << "This program expects an internet connection to work properly." << std::endl;
        std::cout << "On error, the exception will be printed (to stderr) and the return value will be non-zero." << std::endl;
        std::cout << "You may risk data corruption if you \"kill -9\" the process while using the --long-poll" << std::endl;
        std::cout << "    parameter with list-events, only use SIGINT or Ctrl-C to stop the program (give up to 20sec)." << std::endl;
        throw std::runtime_error("No arguments given.\n");
    }
    int reqStep = 0;
    bool longPolling = false;
    bool network = true;
    std::string endpoint;
    bool dumpAll = false;
    bool pretty = false;
    std::string givenQuery;
    int timeout = 20;
    std::string val = "";
    std::string key = "";
    for(int i = 1; i < argc; i++) {
        auto param = std::string(argv[i]);
        if(param == "--long-poll") {
            longPolling = true;
            continue;
        }
        if(param == "--timeout") {
            i++;
            if(i == argc) throw std::runtime_error("Expected a timeout.");
            timeout = (int)strtol(argv[i], nullptr, 10);
            continue;
        }
        if(param == "--val") {
            i++;
            if(i == argc) throw std::runtime_error("Expected a column for result values.");
            val = argv[i];
            continue;
        }
        if(param == "--key") {
            i++;
            if(i == argc) throw std::runtime_error("Expected a column for object keys.");
            key = argv[i];
            continue;
        }
        if(param == "--pretty") {
            pretty = true;
            continue;
        }
        if(reqStep == 2) {
            std::cerr << "Ignoring unknown argument " << param << "." << std::endl;
            continue;
        }
        if(reqStep == 1) {
            if(param == "dump") {
                dumpAll = true;
            } else if(param == "do") {
                dumpAll = false;
                i++;
                if(i == argc) throw std::runtime_error("Expected an SQL query.\n");
                givenQuery = argv[i];
            } else {
                throw std::runtime_error("Expected dump or do.");
            }
            reqStep++;
            continue;
        }
        if(reqStep == 0) {
            if(param == "list-events") {
                endpoint = "events";
            } else if(param == "list-events-no-network") {
                endpoint = "events";
                network = false;
            } else if(param == "detail-event") {
                i++;
                if(i == argc) throw std::runtime_error("Expected an SKU.\n");
                param = std::string(argv[i]);
                std::transform(param.begin(), param.end(), param.begin(), [](unsigned char c) {
                    return std::tolower(c);
                });
                endpoint = "event/" + param;
            } else {
                throw std::runtime_error("Expected list-events or detail-event <SKU>.");
            }
            reqStep++;
            continue;
        }
    }
    if(reqStep != 2) {
        throw std::runtime_error("Not enough arguments");
    }
    //This has to be a bit of a hack, the "events" database is singleton, program-wide,
    //where-as the per-event database is stored in memory and properly scoped, and yet
    //we want to treat them the same way (ScopedSQLite3& db).
    std::unique_ptr<ScopedSQLite3> eventDestructor;
    ScopedSQLite3* dbPtr;
    if(endpoint == "events") {
        eventDestructor = nullptr;
        dbPtr = &dbForEventList(network);
    } else {
        eventDestructor = std::make_unique<ScopedSQLite3>(dbForEndpoint(endpoint));
        dbPtr = &(*eventDestructor);
    }
    ScopedSQLite3& db = *dbPtr;
    //Now that the database is loaded, generate the JSON asked for.
    json orig;
    if(dumpAll) {
        orig = dumpEntireDatabase(db);
    } else {
        orig = dumpQueryResults(db, givenQuery.c_str());
        applyXform(orig, key, val);
    }
    if(pretty) {
        std::cout << orig.dump(2) << std::endl;
    } else {
        std::cout << orig << std::endl;
    }
    if(longPolling) {
        while(true) {
            if(shouldICrash.load()) throw std::runtime_error("Early exit requested.\n");
            bool isThereNewData = endpoint == "events" ? updateEventList(timeout) : db.applyEndpoint(endpoint, timeout);
            if(shouldICrash.load()) throw std::runtime_error("Early exit requested.\n");
            if (isThereNewData) {
                json newData = dumpAll ? dumpEntireDatabase(db) : dumpQueryResults(db, givenQuery.c_str());
                if(!dumpAll) applyXform(newData, key, val);
                std::cout << nlohmann::json::diff(orig, newData);
                orig = newData;
            }
        }
    }
}

int main(int argc, char** argv) {
    setIntHandler();
    try {
        doCLI(argc, argv);
        return 0;
    } catch(const std::exception& err) {
        fprintf(stderr, "You broke it >.<\n");
        fprintf(stderr, "Exception text: %s\n", err.what());
        return -1;
    } catch(...) {
        fprintf(stderr, "You broke it >.<\n");
        fprintf(stderr, "No exception information (what was thrown wasn't a std::exception).\n");
        return -1;
    }
}