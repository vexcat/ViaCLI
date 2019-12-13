#include <string>
#include <sstream>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>

extern std::atomic<bool> shouldICrash;
extern std::condition_variable threadNotifier;

const std::string api = "https://data.vexvia.dwabtech.com/api/v3/";
//const std::string api = "https://google.com/api/v3/";

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
    bool wasSuccessful = true;
    std::thread dlThread([&]() {
        curlpp::Easy request;
        std::cerr << url << std::endl;
        request.setOpt<curlpp::options::Url>(url);
        request.setOpt<curlpp::options::WriteStream>(&os);
        request.setOpt<curlpp::options::Encoding>("identity, br, gzip, deflate");
        request.perform();
        auto code = curlpp::infos::ResponseCode::get(request);
        wasSuccessful = code == 200 || code == 204;
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
    if(!wasSuccessful) {
        throw std::runtime_error("API did not send response code of 200 or 204.\n");
    }
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