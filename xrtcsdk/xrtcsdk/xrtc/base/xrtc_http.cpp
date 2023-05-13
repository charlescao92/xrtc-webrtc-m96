#include "xrtc/base/xrtc_http.h"

#include <rtc_base/logging.h>
#include <rtc_base/thread.h>
#include <rtc_base/time_utils.h>
#include <rtc_base/task_utils/to_queued_task.h>

#include "xrtc/base/xrtc_global.h"

namespace xrtc {

HttpRequestTask::HttpRequestTask(CURLM* curlm, const HttpRequest& request, std::function<void(HttpReply)> resp_func) :
	curlm_(curlm),
	curl_(curl_easy_init()),
	resp_func_(resp_func),
	start_time_(rtc::Time32()),
	request_(request) {
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, false);

	if (request_.get_method() == HttpRequest::HttpMethod::kPost) {
		curl_easy_setopt(curl_, CURLOPT_URL, request_.get_url().c_str());
		curl_easy_setopt(curl_, CURLOPT_POST, 1);
		// 设置http发送的内容类型为JSON
		curl_slist* plist = curl_slist_append(NULL, "Content-Type:application/x-www-form-urlencoded;");
		curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, plist);
		curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, request_.get_body().c_str());
		curl_easy_setopt(curl_, CURLOPT_READFUNCTION, NULL);
		curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, HttpRequestTask::OnWriteData);
		curl_easy_setopt(curl_, CURLOPT_WRITEDATA, this);
		curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT_MS, 15000);
		curl_easy_setopt(curl_, CURLOPT_TIMEOUT, request_.get_timeout());
	}
	else if (request_.get_method() == HttpRequest::HttpMethod::kGet) {
		curl_easy_setopt(curl_, CURLOPT_WRITEDATA, this);
		curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, HttpRequestTask::OnWriteData);
		curl_easy_setopt(curl_, CURLOPT_URL, request_.get_url().c_str());
		curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT_MS, 15000);
		curl_easy_setopt(curl_, CURLOPT_TIMEOUT, request_.get_timeout());
	}
	else {
		return;
	}

	curl_multi_add_handle(curlm_, curl_);
	curl_multi_wakeup(curlm_);
}

HttpRequestTask::~HttpRequestTask() {
	curl_multi_remove_handle(curlm_, curl_);
	curl_easy_cleanup(curl_);
	curl_ = nullptr;
}

size_t HttpRequestTask::OnWriteData(void* buffer, size_t size, size_t nmemb, HttpRequestTask* task) {
	if (!task) {
		return -1;
	}
	return task->OnHttpRequestTaskWriteData(buffer, size, nmemb);
}

size_t HttpRequestTask::OnHttpRequestTaskWriteData(void* buffer, size_t size, size_t nmemb) {
	resp_.append((char*)buffer, size * nmemb);
	return nmemb;
}

HttpReply HttpRequestTask::Resp(int err, const std::string& err_msg, long status_code) {
	uint32_t duration = rtc::Time32() - start_time_;
	HttpReply reply;
	reply.set_error(err);
	reply.set_err_msg(err_msg);
	reply.set_duration(duration);
	reply.set_resp(resp_);
	reply.set_url(request_.get_url());
	reply.set_body(request_.get_body());
	reply.set_status_code(status_code);
	reply.set_form(request_.get_form());
	reply.set_obj(request_.get_obj());

	return reply;
}

HttpManager::HttpManager() {
	curl_global_init(CURL_GLOBAL_ALL);
	multi_ = curl_multi_init();
	/* Limit the amount of simultaneous connections curl should allow: */
	curl_multi_setopt(multi_, CURLMOPT_MAXCONNECTS, (long)100);
}

HttpManager::~HttpManager() {
	if (multi_) {
		curl_multi_cleanup(multi_);
		multi_ = nullptr;
	}

	if (http_thread_) {
		delete http_thread_;
		http_thread_ = nullptr;
	}
}

std::string HttpManager::UrlEncode(const std::string& content) {
	char* new_content = curl_easy_escape(nullptr, content.c_str(), content.length());
	std::string res = new_content;
	curl_free(new_content);
	return res;
}

void HttpManager::Get(HttpRequest request, std::function<void(HttpReply)> resp, void* obj) {
	std::unique_lock<std::mutex> auto_lock(mutex_);
	request.set_method(HttpRequest::HttpMethod::kGet);
	request.set_obj(obj);
	request_list_.push_back(std::make_shared<HttpRequestTask>(multi_, request, resp));
	curl_multi_wakeup(multi_);
}

void HttpManager::Post(HttpRequest request, std::function<void(HttpReply)> resp, void* obj) {
	std::unique_lock<std::mutex> auto_lock(mutex_);
	request.set_method(HttpRequest::HttpMethod::kPost);
	request.set_obj(obj);
	request_list_.push_back(std::make_shared<HttpRequestTask>(multi_, request, resp));
	curl_multi_wakeup(multi_);
}

void HttpManager::AddObject(void* obj) {
	XRTCGlobal::Instance()->worker_thread()->PostTask(webrtc::ToQueuedTask([=]() {
		async_objs_.insert(obj);
		}));
}

void HttpManager::RemoveObject(void* obj) {
	XRTCGlobal::Instance()->worker_thread()->PostTask(webrtc::ToQueuedTask([=]() {
		async_objs_.erase(obj);
		}));
}

void HttpManager::Start() {
	RTC_LOG(LS_INFO) << "HttpManager Start";
	if (running_) {
		return;
	}

	running_ = true;
	http_thread_ = new std::thread([=]() {
		rtc::SetCurrentThreadName("http_thread");

		CURLMsg* msg = nullptr;
		int still_alive = 0;
		int msgs_left;
		int index = 0;

		do {
			CURLMcode mc = curl_multi_poll(multi_, NULL, 0, 10000, NULL);
			index++;
			curl_multi_perform(multi_, &still_alive);

			while ((msg = curl_multi_info_read(multi_, &msgs_left))) {
				if (msg->msg == CURLMSG_DONE) {
					std::shared_ptr<HttpRequestTask> req = GetHttpRequestTask(msg->easy_handle);
					if (req != nullptr) {
						int errNo = msg->data.result;
						std::string errMsg = curl_easy_strerror(msg->data.result);
						long status_code = 0;
						curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &status_code);

						HttpReply reply = req->Resp(errNo, errMsg, status_code);
						std::function<void(HttpReply)> resp_func = req->get_resp_func();

						void* obj = reply.get_obj();
						XRTCGlobal::Instance()->worker_thread()->PostTask(webrtc::ToQueuedTask([=]() {
							for (auto alive_obj : async_objs_) {
								if (obj == alive_obj && resp_func) {
									resp_func(reply);
								}
							}
							}));
					}
					break;
				}
			}
		} while (running_);
		});
}

void HttpManager::Stop() {
	RTC_LOG(LS_INFO) << "HttpManager Stop";
	running_ = false;

	if (http_thread_ && http_thread_->joinable()) {
		curl_multi_wakeup(multi_);
		http_thread_->join();
		RTC_LOG(LS_INFO) << "HttpManager join finished";
	}

	RTC_LOG(LS_INFO) << "HttpManager Stop Finished";
}

std::shared_ptr<HttpRequestTask> HttpManager::GetHttpRequestTask(CURL* handle) {
	std::unique_lock<std::mutex> auto_lock(mutex_);
	for (auto itor = request_list_.begin(); itor != request_list_.end(); ++itor) {
		if (handle == (*itor)->get_curl_handle()) {
			std::shared_ptr<HttpRequestTask> req = std::move(*itor);
			request_list_.erase(itor);
			return req;
		}
	}
	return nullptr;
}

} // namespace xrtc
