// util.hpp - small helpers: JSON writer, CSV parser, string tools, math helpers.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hc {

using Vec = std::vector<double>;
using Mat = std::vector<Vec>;
constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
inline bool isnan_(double x) { return x != x; }

// ------------------------------------------------------------------ settings
// Every tunable value lives here (mirrors config.py of the Python version).
struct Config {
    unsigned seed = 42;
    int lsa_components = 15;
    double w_text = 1.0, w_num = 0.2, w_cat = 0.2;
    double tfidf_max_df = 0.7;
    int tfidf_min_df = 2;
    std::vector<std::string> linkages = {"ward", "complete", "average", "single"};
    int k_min = 2, k_max = 12;
    std::vector<int> min_pts_grid = {3, 4, 5, 8};
    int eps_points = 18;
    double max_noise = 0.30;
    int min_clusters = 3, max_clusters = 15;
    int n_subsamples = 30;
    double subsample_frac = 0.8;
};
inline const Config& cfg() { static Config c; return c; }

// ---------------------------------------------------------------------- JSON
namespace J {
inline std::string str(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
                else o += (char)c;
        }
    }
    return o + "\"";
}
inline std::string num(double v) {
    if (!std::isfinite(v)) return "null";
    std::ostringstream os; os << std::setprecision(10) << v; return os.str();
}
inline std::string boolean(bool b) { return b ? "true" : "false"; }
using KV = std::vector<std::pair<std::string, std::string>>;
inline std::string obj(const KV& kv) {
    std::string o = "{";
    for (size_t i = 0; i < kv.size(); ++i) { if (i) o += ","; o += str(kv[i].first) + ":" + kv[i].second; }
    return o + "}";
}
template <class T, class F> std::string arr(const std::vector<T>& v, F f) {
    std::string o = "[";
    for (size_t i = 0; i < v.size(); ++i) { if (i) o += ","; o += f(v[i]); }
    return o + "]";
}
inline std::string ints(const std::vector<int>& v) { return arr(v, [](int x) { return std::to_string(x); }); }
inline std::string nums(const Vec& v) { return arr(v, [](double x) { return num(x); }); }
inline std::string strs(const std::vector<std::string>& v) { return arr(v, [](const std::string& s) { return str(s); }); }
}  // namespace J

// ------------------------------------------------------------------- strings
inline std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}
inline std::string lower(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }
// Cut to n bytes without splitting a UTF-8 character.
inline std::string utf8_cut(const std::string& s, size_t n) {
    if (s.size() <= n) return s;
    while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) --n;
    return s.substr(0, n);
}
inline bool is_missing_token(const std::string& raw) {
    std::string t = lower(trim(raw));
    return t.empty() || t == "na" || t == "n/a" || t == "nan" || t == "null" || t == "none";
}

inline std::vector<std::vector<std::string>> parse_csv(const std::string& text) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string field;
    bool inq = false;
    size_t i = 0;
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) i = 3;
    for (; i < text.size(); ++i) {
        char c = text[i];
        if (inq) {
            if (c == '"') { if (i + 1 < text.size() && text[i + 1] == '"') { field += '"'; ++i; } else inq = false; }
            else field += c;
        } else if (c == '"') inq = true;
        else if (c == ',') { row.push_back(field); field.clear(); }
        else if (c == '\n' || c == '\r') {
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
            row.push_back(field); field.clear();
            if (!(row.size() == 1 && row[0].empty())) rows.push_back(row);
            row.clear();
        } else field += c;
    }
    if (!field.empty() || !row.empty()) { row.push_back(field); rows.push_back(row); }
    return rows;
}

inline std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}
inline void write_file(const std::string& path, const std::string& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot write " + path);
    f << data;
}

// ---------------------------------------------------------------------- math
inline double median(Vec v) {
    v.erase(std::remove_if(v.begin(), v.end(), [](double x) { return isnan_(x); }), v.end());
    if (v.empty()) return NaN;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}
inline double percentile(Vec v, double p) {  // linear interpolation, like numpy
    std::sort(v.begin(), v.end());
    double pos = p / 100.0 * (v.size() - 1);
    size_t lo = (size_t)std::floor(pos), hi = std::min(v.size() - 1, lo + 1);
    return v[lo] + (v[hi] - v[lo]) * (pos - lo);
}

// Eigen-decomposition of a small symmetric matrix (cyclic Jacobi). Descending order.
inline void jacobi_eigen(Mat A, Vec& w, Mat& V) {
    int n = (int)A.size();
    V.assign(n, Vec(n, 0.0));
    for (int i = 0; i < n; ++i) V[i][i] = 1.0;
    for (int sweep = 0; sweep < 100; ++sweep) {
        double off = 0;
        for (int p = 0; p < n; ++p) for (int q = p + 1; q < n; ++q) off += A[p][q] * A[p][q];
        if (off < 1e-24) break;
        for (int p = 0; p < n; ++p)
            for (int q = p + 1; q < n; ++q) {
                if (std::fabs(A[p][q]) < 1e-300) continue;
                double theta = (A[q][q] - A[p][p]) / (2 * A[p][q]);
                double t = (theta >= 0 ? 1.0 : -1.0) / (std::fabs(theta) + std::sqrt(theta * theta + 1));
                double c = 1 / std::sqrt(t * t + 1), s = t * c;
                for (int k = 0; k < n; ++k) { double a = A[k][p], b = A[k][q]; A[k][p] = c * a - s * b; A[k][q] = s * a + c * b; }
                for (int k = 0; k < n; ++k) { double a = A[p][k], b = A[q][k]; A[p][k] = c * a - s * b; A[q][k] = s * a + c * b; }
                for (int k = 0; k < n; ++k) { double a = V[k][p], b = V[k][q]; V[k][p] = c * a - s * b; V[k][q] = s * a + c * b; }
            }
    }
    std::vector<int> ord(n); std::iota(ord.begin(), ord.end(), 0);
    std::sort(ord.begin(), ord.end(), [&](int a, int b) { return A[a][a] > A[b][b]; });
    w.assign(n, 0); Mat V2(n, Vec(n));
    for (int j = 0; j < n; ++j) { w[j] = A[ord[j]][ord[j]]; for (int i = 0; i < n; ++i) V2[i][j] = V[i][ord[j]]; }
    V = V2;
}

}  // namespace hc
