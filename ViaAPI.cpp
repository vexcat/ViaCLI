#include <string>
#include <sstream>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Options.hpp>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>

extern std::atomic<bool> shouldICrash;
extern std::condition_variable threadNotifier;

const std::string api = "https://data.vexvia.dwabtech.com/api/v3/";

//CurlPP uses a global object to init/destruct itself.
#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
curlpp::Cleanup curlRAII;
#pragma clang diagnostic pop

std::string download(const std::string& url) {
    std::mutex m;
    std::atomic_bool doneVar;
    doneVar.store(false);
    std::ostringstream os;
    std::thread dlThread([&]() {
        std::cerr << url << std::endl;
        os << curlpp::options::Url(std::string(url));
        doneVar.store(true);
        threadNotifier.notify_all();
    });
    std::unique_lock<std::mutex> lk(m);
    bool broskis;
    threadNotifier.wait(lk, [&] { return (broskis = doneVar.load()) || shouldICrash.load(); });
    if(!broskis) {
        dlThread.detach();
        throw std::runtime_error("Early exit requesting during download.\n");
    }
    lk.unlock();
    dlThread.join();
    return os.str();
}

std::string apiCall(const std::string& endpoint, int lastModified, int schema, int timeout) {
    auto ret = download(api + endpoint
                        + "?since=" + std::to_string(lastModified)
                        + "&schema=" + std::to_string(schema)
                        + "&timeout=" + std::to_string(timeout));
    auto lines = std::count(ret.begin(), ret.end(), '\n');
    std::cerr << "Got " << lines << " lines of SQL data." << std::endl;
    return ret;
}