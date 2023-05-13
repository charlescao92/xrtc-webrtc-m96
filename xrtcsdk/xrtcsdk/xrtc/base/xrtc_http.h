#ifndef KRTCSDK_KRTC_BASE_KRTC_HTTP_H_
#define KRTCSDK_KRTC_BASE_KRTC_HTTP_H_

#include <string>
#include <map>
#include <thread>
#include <list>
#include <atomic>
#include <functional>
#include <mutex>
#include <memory>
#include <set>

#include <curl/curl.h>

namespace xrtc {

class HttpRequest {
public:
    enum class HttpMethod {
        kGet = 0,
        kPost,
        kPostForm
    };

    HttpRequest() {}
    HttpRequest(const std::string& url, int timeout_s = 10) :
        url_(url), timeout_s_(timeout_s) {
    }
    HttpRequest(const std::string& url, const std::string& body, int timeout_s = 10) :
        url_(url), body_(body), timeout_s_(timeout_s) {
    }

    ~HttpRequest() {}

    std::string& get_url() { return url_; }
    int get_timeout() { return timeout_s_; }
    std::string& get_body() { return body_; }

    void set_encode_body(const std::string& encode_body) { encode_body_ = encode_body; }
    std::string& get_encode_body() { return encode_body_; }

    void set_method(HttpMethod method) { method_ = method; }
    HttpMethod get_method() { return method_; }

    void set_form(std::map<std::string, std::string> form) { form_ = form; }
    std::map<std::string, std::string> get_form() { return form_; }

    void set_obj(void* obj) { obj_ = obj; }
    void* get_obj() { return obj_; }

private:
    std::string url_;
    int timeout_s_ = 10;
    std::string body_;
    std::string encode_body_;
    std::map<std::string, std::string> form_;
    HttpMethod method_ = HttpMethod::kGet;
    void* obj_ = nullptr;
};

class HttpReply {
public:
    HttpReply() = default;
    ~HttpReply() = default;

    void set_duration(int duration) { duration_ = duration; }
    uint32_t get_duration() { return duration_; }

    void set_error(int err) { err_ = err; }
    int get_errno() const { return err_; }

    void set_err_msg(const std::string& err_msg) { err_msg_ = err_msg; }
    std::string get_err_msg() { return err_msg_; }

    void set_resp(const std::string& resp) { resp_ = resp; }
    std::string get_resp() const { return resp_; }

    void set_url(const std::string& url) { url_ = url; }
    std::string get_url() { return url_; }

    void set_body(const std::string& body) { body_ = body; }
    std::string get_body() { return body_; }

    void set_status_code(long status_code) { status_code_ = status_code; }
    long get_status_code() const { return status_code_; }

    void set_form(const std::map<std::string, std::string>& form) { form_ = form; }
    std::map<std::string, std::string> get_form() { return form_; }

    void set_obj(void* obj) { obj_ = obj; }
    void* get_obj() { return obj_; }

private:
    uint32_t duration_ = 0;
    int err_ = 0;
    std::string err_msg_;
    std::string resp_;
    std::string url_;
    std::string body_;
    long status_code_ = 0;
    std::map<std::string, std::string> form_;
    void* obj_ = nullptr;
};

class HttpRequestTask {
public:
    HttpRequestTask(CURLM* curlm, const HttpRequest& request, std::function<void(HttpReply)> resp_func);
    ~HttpRequestTask();

    HttpReply Resp(int err, const std::string& err_msg, long status_code);
    std::function<void(HttpReply)> get_resp_func() { return resp_func_; }
    CURL* get_curl_handle() { return curl_; }

protected:
    static size_t OnWriteData(void* buffer, size_t size, size_t nmemb, HttpRequestTask* _this);
    size_t OnHttpRequestTaskWriteData(void* buffer, size_t size, size_t nmemb);

private:
    CURL* curl_ = nullptr;
    CURLM* curlm_ = nullptr;
    std::string resp_;
    std::function<void(HttpReply)> resp_func_;
    uint32_t start_time_ = 0;
    HttpRequest request_;
};

class HttpManager {
public:
    HttpManager();
    ~HttpManager();

    static std::string UrlEncode(const std::string& content);

    void Start();
    void Stop();

    void Get(HttpRequest request, std::function<void(HttpReply)> resp, void* obj);
    void Post(HttpRequest request, std::function<void(HttpReply)> resp, void* obj);

    void AddObject(void* obj);
    void RemoveObject(void* obj);

private:
    std::shared_ptr<HttpRequestTask> GetHttpRequestTask(CURL* handle);

private:
    std::mutex mutex_;
    CURLM* multi_ = nullptr;
    std::thread* http_thread_ = nullptr;
    std::list<std::shared_ptr<HttpRequestTask>> request_list_;
    std::atomic<bool> running_{ false };
    std::set<void*> async_objs_;
};

} // namespace xrtc

#endif // KRTCSDK_KRTC_BASE_KRTC_HTTP_H_