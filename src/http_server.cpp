// http_server.cpp: see http_server.hpp.
#include "http_server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#include "map.hpp"
#include "schedules_api.hpp"
#include "snapshot.hpp"

namespace tribes {

namespace {

HttpResponse json_error(int status, const std::string& msg) {
    HttpResponse r;
    r.status = status;
    r.body = "{\"error\":\"" + msg + "\"}";
    return r;
}

std::string url_decode(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size() + 0 && std::isxdigit((unsigned char)s[i + 1]) &&
            std::isxdigit((unsigned char)s[i + 2])) {
            o += static_cast<char>(std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16));
            i += 2;
        } else o += s[i];
    }
    return o;
}

bool query_int(const std::string& q, const char* key, int* out) {
    const std::string k = std::string(key) + "=";
    size_t pos = 0;
    while (pos <= q.size()) {
        size_t amp = q.find('&', pos);
        std::string part = q.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
        if (part.compare(0, k.size(), k) == 0) {
            char* end = nullptr;
            long v = std::strtol(part.c_str() + k.size(), &end, 10);
            if (end == part.c_str() + k.size() || *end) return false;
            if (v < INT_MIN / 2 || v > INT_MAX / 2) return false;
            *out = static_cast<int>(v);
            return true;
        }
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
    return false;
}

std::string mime_of(const std::string& p) {
    auto ends = [&](const char* e) {
        size_t n = std::strlen(e);
        return p.size() >= n && p.compare(p.size() - n, n, e) == 0;
    };
    if (ends(".html")) return "text/html; charset=utf-8";
    if (ends(".js") || ends(".mjs")) return "text/javascript; charset=utf-8";
    if (ends(".css")) return "text/css; charset=utf-8";
    if (ends(".json")) return "application/json; charset=utf-8";
    if (ends(".svg")) return "image/svg+xml";
    if (ends(".png")) return "image/png";
    if (ends(".jpg") || ends(".jpeg")) return "image/jpeg";
    if (ends(".ico")) return "image/x-icon";
    if (ends(".txt") || ends(".md")) return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

// Tiny parser for {"action":"...","value":N}.
bool parse_control(const std::string& b, std::string* action, double* value,
                   bool* has_value) {
    auto find_val = [&](const char* key) -> size_t {
        size_t p = b.find(std::string("\"") + key + "\"");
        if (p == std::string::npos) return p;
        p = b.find(':', p);
        if (p == std::string::npos) return p;
        ++p;
        while (p < b.size() && std::isspace((unsigned char)b[p])) ++p;
        return p;
    };
    size_t p = find_val("action");
    if (p == std::string::npos || p >= b.size() || b[p] != '"') return false;
    size_t e = b.find('"', p + 1);
    if (e == std::string::npos) return false;
    *action = b.substr(p + 1, e - p - 1);
    *has_value = false;
    size_t v = find_val("value");
    if (v != std::string::npos && v < b.size()) {
        char* end = nullptr;
        double d = std::strtod(b.c_str() + v, &end);
        if (end != b.c_str() + v) { *value = d; *has_value = true; }
    }
    return true;
}

bool host_ok(const std::string& host) {
    // Only loopback names: blocks DNS-rebinding from another site.
    std::string h = host;
    size_t c = h.rfind(':');
    if (c != std::string::npos) h = h.substr(0, c);
    return h == "localhost" || h == "127.0.0.1" || h == "[::1]" || h.empty();
}

bool origin_ok(const HttpRequest& r) {
    if (r.origin.empty()) return true;
    size_t p = r.origin.find("://");
    std::string o = p == std::string::npos ? r.origin : r.origin.substr(p + 3);
    return o == r.host;
}

}  // namespace

int resolve_static(const std::string& web_root, const std::string& url_path,
                   std::string* out) {
    std::string p = url_decode(url_path);
    if (p.find('\0') != std::string::npos) return 403;
    if (p.empty() || p[0] != '/') return 403;
    // Reject any ".." component outright, and backslashes.
    if (p.find('\\') != std::string::npos) return 403;
    size_t i = 0;
    while (i <= p.size()) {
        size_t j = p.find('/', i);
        std::string seg = p.substr(i, j == std::string::npos ? std::string::npos : j - i);
        if (seg == "..") return 403;
        if (j == std::string::npos) break;
        i = j + 1;
    }
    if (p == "/" || p.back() == '/') p += "index.html";
    char* root_real = ::realpath(web_root.c_str(), nullptr);
    if (!root_real) return 404;
    std::string root(root_real);
    std::free(root_real);
    char* file_real = ::realpath((root + p).c_str(), nullptr);
    if (!file_real) return 404;
    std::string file(file_real);
    std::free(file_real);
    if (file.compare(0, root.size() + 1, root + "/") != 0) return 403;
    struct stat st;
    if (::stat(file.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return 404;
    *out = file;
    return 200;
}

HttpResponse handle_request(World& w, const HttpRequest& req,
                            const std::string& web_root) {
    if (!host_ok(req.host)) return json_error(403, "bad host");
    std::string path = req.target, query;
    size_t q = path.find('?');
    if (q != std::string::npos) {
        query = path.substr(q + 1);
        path = path.substr(0, q);
    }
    if (req.method == "POST") {
        if (path != "/api/control") return json_error(404, "not found");
        if (!origin_ok(req)) return json_error(403, "cross-origin request refused");
        std::string action;
        double value = 0;
        bool has_value = false;
        if (!parse_control(req.body, &action, &value, &has_value))
            return json_error(400, "expected {\"action\":\"pause|resume|speed\",\"value\":seconds}");
        if (action == "pause") {
            w.paused.store(true);
        } else if (action == "resume") {
            w.paused.store(false);
        } else if (action == "speed") {
            if (!has_value || !(value > 0) || value > 3600)
                return json_error(400, "speed needs value in (0, 3600] seconds per day");
            std::lock_guard<std::mutex> g(w.lock);
            w.tick_interval = std::max(0.01, value);
        } else {
            return json_error(400, "unknown action");
        }
        HttpResponse r;
        std::lock_guard<std::mutex> g(w.lock);
        r.body = std::string("{\"ok\":true,\"paused\":") +
                 (w.paused.load() ? "true" : "false") +
                 ",\"seconds_per_day\":" + std::to_string(w.tick_interval) + "}";
        return r;
    }
    if (req.method != "GET" && req.method != "HEAD")
        return json_error(405, "method not allowed");

    HttpResponse r;
    if (path == "/api/state") {
        r.body = json_state(take_snapshot(w));
    } else if (path == "/api/people") {
        int since = 0;
        bool has = false;
        if (!query.empty()) {
            has = query_int(query, "since", &since);
            if (query.find("since=") != std::string::npos && !has)
                return json_error(400, "since must be an integer day");
        }
        r.body = json_people(take_snapshot(w), has, since);
    } else if (path.compare(0, 12, "/api/person/") == 0) {
        std::string idstr = path.substr(12);
        char* end = nullptr;
        long id = std::strtol(idstr.c_str(), &end, 10);
        if (idstr.empty() || *end || id < 0 || id > INT_MAX / 2)
            return json_error(400, "bad person id");
        PersonDetail d = take_person(w, static_cast<int>(id));
        if (!d.found) return json_error(404, "no such person");
        r.body = json_person(d);
    } else if (path == "/api/market") {
        r.body = json_market(take_snapshot(w));
    } else if (path == "/api/herds") {
        r.body = json_herds(take_snapshot(w));
    } else if (path == "/api/caravans") {
        r.body = json_caravans(take_snapshot(w));
    } else if (path == "/api/schedules") {
        // One day's schedules for everyone, built once per day and cached.
        static std::string cached_s;
        static int cached_day = -1;
        static uint64_t cached_key = 0;
        MapInputs in = take_map_inputs(w);
        static ValleyMap vm;
        static size_t vm_wells = static_cast<size_t>(-1);
        static uint64_t vm_seed = 0;
        if (vm_wells != in.well_tribes.size() || vm_seed != in.seed) {
            vm = build_map(in);
            vm_wells = in.well_tribes.size();
            vm_seed = in.seed;
        }
        int day = 0;
        {
            std::lock_guard<std::mutex> g(w.lock);
            day = w.tick;
        }
        int asked = day;
        if (query_int(query, "day", &asked) && asked != day) day = asked;
        const uint64_t key = in.seed * 1000003ULL + in.well_tribes.size();
        if (cached_day != day || cached_key != key || cached_s.empty()) {
            cached_s = json_schedules(w, vm, day);
            cached_day = day;
            cached_key = key;
        }
        r.body = cached_s;
    } else if (path == "/api/map") {
        // The layout is built once and cached; it is rebuilt only when the
        // simulation digs a new well (the server runs on one thread).
        static std::string cached;
        static size_t cached_wells = static_cast<size_t>(-1);
        static uint64_t cached_seed = 0;
        MapInputs in = take_map_inputs(w);
        if (cached.empty() || cached_wells != in.well_tribes.size() || cached_seed != in.seed) {
            cached = json_valley_map(build_map(in));
            cached_wells = in.well_tribes.size();
            cached_seed = in.seed;
        }
        r.body = cached;
    } else if (path.compare(0, 5, "/api/") == 0) {
        return json_error(404, "not found");
    } else {
        std::string file;
        int st = resolve_static(web_root, path, &file);
        if (st != 200) {
            r.status = st;
            r.content_type = "text/plain; charset=utf-8";
            r.body = st == 403 ? "403 forbidden\n" : "404 not found\n";
            return r;
        }
        std::ifstream in(file, std::ios::binary);
        std::ostringstream ss;
        ss << in.rdbuf();
        r.body = ss.str();
        r.content_type = mime_of(file);
    }
    return r;
}

// --- sockets ------------------------------------------------------------------------

HttpServer::HttpServer(World& w, int port, std::string web_root)
    : w_(w), port_(port), web_root_(std::move(web_root)) {}

HttpServer::~HttpServer() { stop(); }

bool HttpServer::start(std::string* error) {
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) { *error = std::strerror(errno); return false; }
    int one = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(static_cast<uint16_t>(port_));
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // 127.0.0.1 only
    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0 ||
        ::listen(listen_fd_, 16) != 0) {
        *error = std::strerror(errno);
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }
    running_ = true;
    thread_ = std::thread([this] { loop(); });
    return true;
}

void HttpServer::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();
    if (listen_fd_ >= 0) ::close(listen_fd_);
    listen_fd_ = -1;
}

void HttpServer::loop() {
    while (running_ && !w_.stop.load()) {
        pollfd p{listen_fd_, POLLIN, 0};
        if (::poll(&p, 1, 200) <= 0) continue;
        int fd = ::accept(listen_fd_, nullptr, nullptr);
        if (fd < 0) continue;
        timeval tv{2, 0};   // a slow client cannot hold the one thread for long
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
        serve_client(fd);
        ::close(fd);
    }
}

static std::string header_of(const std::string& head, const char* name) {
    std::string low = head, key = std::string("\r\n") + name + ":";
    for (auto& c : low) c = static_cast<char>(std::tolower((unsigned char)c));
    size_t p = low.find(key);
    if (p == std::string::npos) return "";
    p += key.size();
    size_t e = head.find("\r\n", p);
    std::string v = head.substr(p, e == std::string::npos ? std::string::npos : e - p);
    size_t a = v.find_first_not_of(" \t"), b = v.find_last_not_of(" \t");
    return a == std::string::npos ? "" : v.substr(a, b - a + 1);
}

void HttpServer::serve_client(int fd) {
    std::string buf;
    char tmp[4096];
    size_t head_end = std::string::npos;
    while (head_end == std::string::npos && buf.size() < 16384) {
        ssize_t n = ::recv(fd, tmp, sizeof tmp, 0);
        if (n <= 0) return;
        buf.append(tmp, static_cast<size_t>(n));
        head_end = buf.find("\r\n\r\n");
    }
    HttpResponse resp;
    if (head_end == std::string::npos) {
        resp = json_error(431, "headers too large");
    } else {
        std::string head = buf.substr(0, head_end + 2);
        HttpRequest req;
        size_t sp1 = head.find(' '), sp2 = head.find(' ', sp1 + 1);
        if (sp1 == std::string::npos || sp2 == std::string::npos) {
            resp = json_error(400, "bad request");
        } else {
            req.method = head.substr(0, sp1);
            req.target = head.substr(sp1 + 1, sp2 - sp1 - 1);
            req.host = header_of(head, "host");
            req.origin = header_of(head, "origin");
            size_t clen = 0;
            std::string cl = header_of(head, "content-length");
            if (!cl.empty()) clen = static_cast<size_t>(std::strtoul(cl.c_str(), nullptr, 10));
            if (clen > 65536) {
                resp = json_error(413, "body too large");
            } else {
                req.body = buf.substr(head_end + 4);
                while (req.body.size() < clen) {
                    ssize_t n = ::recv(fd, tmp, sizeof tmp, 0);
                    if (n <= 0) break;
                    req.body.append(tmp, static_cast<size_t>(n));
                }
                try {
                    resp = handle_request(w_, req, web_root_);
                } catch (...) {
                    resp = json_error(500, "internal error");
                }
            }
        }
        if (req.method == "HEAD") {
            // headers only, but with the true length
        }
        const char* reason = resp.status == 200 ? "OK" : resp.status == 400 ? "Bad Request"
            : resp.status == 403 ? "Forbidden" : resp.status == 404 ? "Not Found"
            : resp.status == 405 ? "Method Not Allowed" : resp.status == 413 ? "Payload Too Large"
            : "Error";
        std::string out = "HTTP/1.1 " + std::to_string(resp.status) + " " + reason +
            "\r\nContent-Type: " + resp.content_type +
            "\r\nContent-Length: " + std::to_string(resp.body.size()) +
            "\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff"
            "\r\nConnection: close\r\n\r\n";
        if (req.method != "HEAD") out += resp.body;
        size_t off = 0;
        while (off < out.size()) {
            ssize_t n = ::send(fd, out.data() + off, out.size() - off, MSG_NOSIGNAL);
            if (n <= 0) break;
            off += static_cast<size_t>(n);
        }
        return;
    }
    std::string out = "HTTP/1.1 " + std::to_string(resp.status) +
        " Error\r\nContent-Type: application/json\r\nContent-Length: " +
        std::to_string(resp.body.size()) + "\r\nConnection: close\r\n\r\n" + resp.body;
    ::send(fd, out.data(), out.size(), MSG_NOSIGNAL);
}

}  // namespace tribes
