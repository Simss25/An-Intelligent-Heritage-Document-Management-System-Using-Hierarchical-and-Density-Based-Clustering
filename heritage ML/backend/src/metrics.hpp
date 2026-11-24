// metrics.hpp - silhouette, ARI, NMI, subsample stability, distinctive terms.
#pragma once
#include "cluster.hpp"
#include "features.hpp"

namespace hc {

// Per-document silhouette; noise (-1) is ignored. NaN where undefined.
inline Vec silhouette_values(const DistMat& D, const std::vector<int>& lab) {
    int n = D.n; Vec s(n, NaN);
    std::vector<int> keep; for (int i = 0; i < n; ++i) if (lab[i] != -1) keep.push_back(i);
    std::map<int, std::vector<int>> groups; for (int i : keep) groups[lab[i]].push_back(i);
    if (keep.size() < 3 || groups.size() < 2) return s;
    for (int i : keep) {
        const auto& own = groups[lab[i]];
        if (own.size() < 2) { s[i] = 0; continue; }
        double a = 0; for (int j : own) if (j != i) a += D.at(i, j);
        a /= (own.size() - 1);
        double b = std::numeric_limits<double>::infinity();
        for (auto& g : groups) { if (g.first == lab[i]) continue; double m = 0; for (int j : g.second) m += D.at(i, j); b = std::min(b, m / g.second.size()); }
        s[i] = (b - a) / std::max(a, b);
    }
    return s;
}
inline double silhouette_score(const DistMat& D, const std::vector<int>& lab) {
    Vec v = silhouette_values(D, lab); double t = 0; int c = 0;
    for (double x : v) if (!isnan_(x)) { t += x; ++c; }
    return c ? t / c : NaN;
}

template <class T> std::vector<int> encode(const std::vector<T>& v) {
    std::map<T, int> m; std::vector<int> o;
    for (auto& x : v) { auto it = m.find(x); if (it == m.end()) it = m.insert({x, (int)m.size()}).first; o.push_back(it->second); }
    return o;
}
inline void contingency(const std::vector<int>& a, const std::vector<int>& b, std::vector<std::vector<double>>& t) {
    int na = *std::max_element(a.begin(), a.end()) + 1, nb = *std::max_element(b.begin(), b.end()) + 1;
    t.assign(na, std::vector<double>(nb, 0));
    for (size_t i = 0; i < a.size(); ++i) t[a[i]][b[i]]++;
}
inline double ari(const std::vector<int>& a0, const std::vector<int>& b0) {
    auto a = encode(a0), b = encode(b0); std::vector<std::vector<double>> t; contingency(a, b, t);
    auto c2 = [](double x) { return x * (x - 1) / 2; };
    double sum_ij = 0, sa = 0, sb = 0, n = (double)a.size();
    Vec cb(t[0].size(), 0);
    for (auto& r : t) { double rs = 0; for (size_t j = 0; j < r.size(); ++j) { sum_ij += c2(r[j]); rs += r[j]; cb[j] += r[j]; } sa += c2(rs); }
    for (double x : cb) sb += c2(x);
    double expected = sa * sb / c2(n), maxi = 0.5 * (sa + sb);
    if (maxi == expected) return 1.0;
    return (sum_ij - expected) / (maxi - expected);
}
inline double nmi(const std::vector<int>& a0, const std::vector<int>& b0) {
    auto a = encode(a0), b = encode(b0); std::vector<std::vector<double>> t; contingency(a, b, t);
    double n = (double)a.size(), mi = 0, ha = 0, hb = 0; Vec cb(t[0].size(), 0), ra(t.size(), 0);
    for (size_t i = 0; i < t.size(); ++i) for (size_t j = 0; j < t[i].size(); ++j) { ra[i] += t[i][j]; cb[j] += t[i][j]; }
    for (size_t i = 0; i < t.size(); ++i) for (size_t j = 0; j < t[i].size(); ++j)
        if (t[i][j] > 0) mi += t[i][j] / n * std::log(t[i][j] * n / (ra[i] * cb[j]));
    for (double x : ra) if (x > 0) ha -= x / n * std::log(x / n);
    for (double x : cb) if (x > 0) hb -= x / n * std::log(x / n);
    double d = 0.5 * (ha + hb);
    return d > 0 ? mi / d : 1.0;
}

struct Stab { double mean = 0, sd = 0; };
// ARI between labels on random subsamples and the full-data labels (same points).
inline Stab stability(const DistMat& D, const std::vector<int>& full,
                      const std::function<std::vector<int>(const DistMat&)>& labeller) {
    const Config& C = cfg();
    std::mt19937 rng(C.seed); int n = D.n, m = (int)(n * C.subsample_frac);
    Vec sc;
    for (int r = 0; r < C.n_subsamples; ++r) {
        std::vector<int> idx(n); std::iota(idx.begin(), idx.end(), 0); std::shuffle(idx.begin(), idx.end(), rng);
        idx.resize(m); std::sort(idx.begin(), idx.end());
        std::vector<int> ref; for (int i : idx) ref.push_back(full[i]);
        sc.push_back(ari(ref, labeller(submatrix(D, idx))));
    }
    double mu = 0; for (double x : sc) mu += x; mu /= sc.size();
    double v = 0; for (double x : sc) v += (x - mu) * (x - mu);
    return {mu, std::sqrt(v / sc.size())};
}

// Terms whose mean TF-IDF inside a cluster most exceeds the corpus mean.
inline std::map<int, std::vector<std::string>> top_terms(const Features& F, const std::vector<int>& lab, int count = 8) {
    int V = (int)F.terms.size(), n = (int)lab.size();
    Vec overall(V, 0.0); for (auto& r : F.tfidf) for (auto& e : r) overall[e.first] += e.second / n;
    std::map<int, Vec> sum; std::map<int, int> cnt;
    for (int i = 0; i < n; ++i) { auto& s = sum[lab[i]]; if (s.empty()) s.assign(V, 0.0); cnt[lab[i]]++; for (auto& e : F.tfidf[i]) s[e.first] += e.second; }
    std::map<int, std::vector<std::string>> out;
    for (auto& kv : sum) {
        Vec diff(V); for (int j = 0; j < V; ++j) diff[j] = kv.second[j] / cnt[kv.first] - overall[j];
        std::vector<int> ord(V); std::iota(ord.begin(), ord.end(), 0);
        std::partial_sort(ord.begin(), ord.begin() + std::min(count, V), ord.end(), [&](int a, int b) { return diff[a] > diff[b]; });
        for (int i = 0; i < std::min(count, V); ++i) out[kv.first].push_back(F.terms[ord[i]]);
    }
    return out;
}

}  // namespace hc
