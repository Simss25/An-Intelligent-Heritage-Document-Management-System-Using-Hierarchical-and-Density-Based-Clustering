// server.hpp - minimal HTTP/1.1 server (POSIX sockets, Winsock on Windows). No external libraries.
#pragma once
#include <mutex>
#include <thread>
#include "pipeline.hpp"

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  using sock_t = SOCKET;
  inline void close_sock(sock_t s) { closesocket(s); }
#else
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <unistd.h>
  using sock_t = int;
  inline void close_sock(sock_t s) { ::close(s); }
#endif

namespace hc {

struct Request { std::string method, path, query, body, content_type; };
struct Response { int status = 200; std::string type = "application/json; charset=utf-8", body; };

inline std::string url_decode(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) { o += (char)std::stoi(s.substr(i + 1, 2), nullptr, 16); i += 2; }
        else if (s[i] == '+') o += ' '; else o += s[i];
    }
    return o;
}
// pulls "key": value out of a flat JSON object (enough for our small POST bodies)
inline std::string json_field(const std::string& body, const std::string& key) {
    size_t p = body.find("\"" + key + "\""); if (p == std::string::npos) return "";
    p = body.find(':', p); if (p == std::string::npos) return "";
    ++p; while (p < body.size() && (body[p] == ' ' || body[p] == '"')) ++p;
    size_t e = p; while (e < body.size() && body[e] != ',' && body[e] != '}' && body[e] != '"' && body[e] != ' ') ++e;
    return body.substr(p, e - p);
}
inline Response err(int status, const std::string& msg) { Response r; r.status = status; r.body = J::obj({{"error", J::str(msg)}}); return r; }

class App {
public:
    App(std::string csv, std::string out, std::string data) : csv_(std::move(csv)), out_(std::move(out)), data_(std::move(data)) { load(csv_); }

    void load(const std::string& path) { auto c = analyze(path, out_, true); std::lock_guard<std::mutex> g(m_); ctx_ = c; }

    Response handle(const Request& rq) {
        std::shared_ptr<Context> c; { std::lock_guard<std::mutex> g(m_); c = ctx_; }
        if (rq.method == "OPTIONS") { Response r; r.status = 204; return r; }
        if (rq.path == "/api/health") return {200, "application/json", J::obj({{"ok", "true"}, {"n_docs", std::to_string(c->D.n)}})};
        if (rq.path == "/api/analysis") return {200, "application/json", c->analysis_json};
        if (rq.path == "/api/documents") return {200, "application/json", c->docs_json};
        if (rq.path == "/api/report") return {200, "text/markdown; charset=utf-8", read_file(out_ + "/report.md")};
        if (rq.path.rfind("/api/figures/", 0) == 0) {
            std::string name = rq.path.substr(13);
            if (name.find("..") != std::string::npos || name.find('/') != std::string::npos || name.find('\\') != std::string::npos) return err(400, "bad name");
            try { return {200, "image/svg+xml", read_file(out_ + "/figures/" + name)}; } catch (...) { return err(404, "no such figure"); }
        }
        if (rq.path == "/api/cluster" && rq.method == "POST") return cluster(*c, rq.body);
        if (rq.path == "/api/search") return search(*c, rq.query);
        if (rq.path == "/api/upload" && rq.method == "POST") return upload(rq);
        return err(404, "not found");
    }

private:
    std::string csv_, out_, data_; std::shared_ptr<Context> ctx_; std::mutex m_;

    Response cluster(Context& c, const std::string& body) {
        std::string method = json_field(body, "method"); std::vector<int> lab;
        try {
            if (method == "hierarchical") {
                std::string link = json_field(body, "linkage"); if (!c.H.count(link)) return err(400, "unknown linkage");
                int k = std::max(2, std::min(20, std::stoi(json_field(body, "k")))); k = std::min(k, c.D.n - 1);
                lab = cut_tree(c.H[link].tree, k);
            } else if (method == "dbscan") {
                double eps = std::max(0.05, std::min(5.0, std::stod(json_field(body, "eps")))); int mp = std::max(2, std::min(30, std::stoi(json_field(body, "min_pts"))));
                lab = dbscan(c.D, eps, mp);
            } else return err(400, "method must be hierarchical or dbscan");
        } catch (...) { return err(400, "invalid parameter"); }
        int nc = 0, nz = 0; for (int l : lab) { nc = std::max(nc, l + 1); nz += l == -1; }
        Response r; r.body = J::obj({{"labels", J::ints(lab)}, {"silhouette", J::num(silhouette_score(c.D, lab))}, {"n_clusters", std::to_string(nc)},
                                     {"noise_frac", J::num((double)nz / lab.size())}, {"clusters", cluster_describe_json(c, lab, false, 5)}});
        return r;
    }

    Response search(Context& c, const std::string& query) {
        std::string q; size_t p = query.find("q="); if (p != std::string::npos) { size_t e = query.find('&', p); q = url_decode(query.substr(p + 2, e == std::string::npos ? e : e - p - 2)); }
        q = clean_text(q); if (q.size() < 3) return {200, "application/json", "[]"};
        std::unordered_map<int, double> qv; double nq = 0;
        std::unordered_map<std::string, int> cnt; for (auto& g : ngrams(q)) cnt[g]++;
        for (auto& kv : cnt) { auto it = c.F.vindex.find(kv.first); if (it == c.F.vindex.end()) continue; double w = (1 + std::log((double)kv.second)) * c.F.idf[it->second]; qv[it->second] = w; nq += w * w; }
        nq = std::sqrt(nq); std::vector<std::pair<double, int>> sc;
        if (nq > 0) for (int i = 0; i < c.D.n; ++i) { double s = 0; for (auto& e : c.F.tfidf[i]) { auto it = qv.find(e.first); if (it != qv.end()) s += e.second * it->second; } if (s > 0) sc.push_back({s / nq, i}); }
        std::sort(sc.begin(), sc.end(), std::greater<>());
        std::vector<std::string> out;
        for (size_t i = 0; i < sc.size() && i < 8; ++i) { const Doc& d = c.docs[sc[i].second];
            out.push_back(J::obj({{"score", J::num(sc[i].first)}, {"doc_id", J::str(d.id)}, {"title", J::str(d.title)}, {"language", d.has_lang ? J::str(d.lang) : "null"}, {"year", J::num(d.year)}})); }
        return {200, "application/json", J::arr(out, [](const std::string& s) { return s; })};
    }

    Response upload(const Request& rq) {
        std::string ct = rq.content_type, b = "boundary="; size_t p = ct.find(b);
        if (p == std::string::npos) return err(400, "expected multipart upload");
        std::string bound = "--" + ct.substr(p + b.size());
        size_t s = rq.body.find("filename=\""); if (s == std::string::npos) return err(400, "no file");
        size_t fn_end = rq.body.find('"', s + 10); if (fn_end == std::string::npos) return err(400, "bad upload");
        std::string fname = lower(rq.body.substr(s + 10, fn_end - s - 10));
        if (fname.size() < 4 || fname.substr(fname.size() - 4) != ".csv") return err(400, "upload a .csv file");
        size_t hs = rq.body.find("\r\n\r\n", s); if (hs == std::string::npos) return err(400, "bad upload");
        size_t he = rq.body.find("\r\n" + bound, hs + 4); if (he == std::string::npos) return err(400, "bad upload");
        std::string dest = data_ + "/uploaded.csv";
        write_file(dest, rq.body.substr(hs + 4, he - hs - 4));
        try { load(dest); } catch (const std::exception& e) { return err(400, std::string("could not analyse file: ") + e.what()); }
        std::shared_ptr<Context> c; { std::lock_guard<std::mutex> g(m_); c = ctx_; }
        return {200, "application/json", J::obj({{"ok", "true"}, {"n_docs", std::to_string(c->D.n)}})};
    }
};

inline void serve_connection(App& app, sock_t cs) {
    std::string buf; char tmp[8192]; size_t hdr_end = std::string::npos;
    while (hdr_end == std::string::npos && buf.size() < (1 << 20)) {
        int n = (int)recv(cs, tmp, sizeof tmp, 0); if (n <= 0) { close_sock(cs); return; }
        buf.append(tmp, n); hdr_end = buf.find("\r\n\r\n");
    }
    if (hdr_end == std::string::npos) { close_sock(cs); return; }
    Request rq; std::istringstream hs(buf.substr(0, hdr_end)); std::string line, target; std::getline(hs, line);
    { std::istringstream rl(line); rl >> rq.method >> target; }
    size_t q = target.find('?'); rq.path = target.substr(0, q); if (q != std::string::npos) rq.query = target.substr(q + 1);
    size_t clen = 0;
    while (std::getline(hs, line)) {
        std::string l = lower(line);
        if (l.rfind("content-length:", 0) == 0) clen = (size_t)std::atoll(l.c_str() + 15);
        if (l.rfind("content-type:", 0) == 0) rq.content_type = trim(line.substr(13));
    }
    if (clen > (64u << 20)) { close_sock(cs); return; }
    rq.body = buf.substr(hdr_end + 4);
    while (rq.body.size() < clen) { int n = (int)recv(cs, tmp, sizeof tmp, 0); if (n <= 0) break; rq.body.append(tmp, n); }
    Response r;
    try { r = app.handle(rq); } catch (const std::exception& e) { r = err(500, e.what()); }
    std::ostringstream out;
    out << "HTTP/1.1 " << r.status << (r.status == 200 ? " OK" : r.status == 204 ? " No Content" : " Error") << "\r\nContent-Type: " << r.type
        << "\r\nContent-Length: " << r.body.size() << "\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Headers: Content-Type\r\n"
        << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\nConnection: close\r\n\r\n" << r.body;
    std::string s = out.str(); size_t sent = 0;
    while (sent < s.size()) { int n = (int)send(cs, s.data() + sent, (int)(s.size() - sent), 0); if (n <= 0) break; sent += n; }
    close_sock(cs);
}

inline int serve(const std::string& csv, const std::string& out, const std::string& data, int port) {
#ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    App app(csv, out, data);
    sock_t ls = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1; setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons((unsigned short)port); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(ls, (sockaddr*)&a, sizeof a) < 0) { std::cerr << "cannot bind port " << port << " (is it already in use?)" << std::endl; return 1; }
    listen(ls, 32);
    std::cout << "Backend ready: http://127.0.0.1:" << port << "/api/health" << std::endl;
    for (;;) {
        sock_t cs = accept(ls, nullptr, nullptr);
        if (cs < 0) continue;
        std::thread([&app, cs] { serve_connection(app, cs); }).detach();
    }
}

}  // namespace hc
