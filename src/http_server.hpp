#pragma once
// A minimal blocking HTTP/1.1 server (POSIX sockets, one thread,
// Connection: close, 127.0.0.1 only, no external code). Same-origin only:
// no CORS headers are ever sent.

#include <atomic>
#include <string>
#include <thread>

#include "world.hpp"

namespace tribes {

struct HttpRequest {
    std::string method, target, body, host, origin;
};

struct HttpResponse {
    int status = 200;
    std::string content_type = "application/json; charset=utf-8";
    std::string body;
};

// Route one request. Pure of sockets, so it is testable directly.
HttpResponse handle_request(World& w, const HttpRequest& req,
                            const std::string& web_root);

// Map a URL path to a file under web_root. Returns 200 and sets *out, or
// 403 (traversal / bad path) or 404.
int resolve_static(const std::string& web_root, const std::string& url_path,
                   std::string* out);

class HttpServer {
public:
    HttpServer(World& w, int port, std::string web_root);
    ~HttpServer();
    bool start(std::string* error);   // binds 127.0.0.1:port, spawns thread
    void stop();
    int port() const { return port_; }
private:
    void loop();
    void serve_client(int fd);
    World& w_;
    int port_;
    std::string web_root_;
    int listen_fd_ = -1;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

}  // namespace tribes
