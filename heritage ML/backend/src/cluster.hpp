// cluster.hpp - agglomerative clustering (4 linkages), DBSCAN, distance helpers.
#pragma once
#include "util.hpp"

namespace hc {

struct DistMat {
    int n = 0;
    std::vector<double> d;  // row-major n*n Euclidean distances
    double at(int i, int j) const { return d[(size_t)i * n + j]; }
};

inline DistMat euclid(const Mat& X) {
    DistMat D; D.n = (int)X.size(); D.d.assign((size_t)D.n * D.n, 0.0);
    for (int i = 0; i < D.n; ++i)
        for (int j = i + 1; j < D.n; ++j) {
            double s = 0; for (size_t k = 0; k < X[i].size(); ++k) { double t = X[i][k] - X[j][k]; s += t * t; }
            D.d[(size_t)i * D.n + j] = D.d[(size_t)j * D.n + i] = std::sqrt(s);
        }
    return D;
}
inline DistMat submatrix(const DistMat& D, const std::vector<int>& idx) {
    DistMat S; S.n = (int)idx.size(); S.d.resize((size_t)S.n * S.n);
    for (int a = 0; a < S.n; ++a) for (int b = 0; b < S.n; ++b) S.d[(size_t)a * S.n + b] = D.at(idx[a], idx[b]);
    return S;
}

// ------------------------------------------------------------- hierarchical
struct Merge { int a, b; double h; int size; };
struct Tree { int n = 0; std::vector<Merge> m; double cophenetic = NaN; std::string method; };

// Lance-Williams updates; node ids >= n are internal nodes created at step (id - n).
inline Tree agglomerate(const DistMat& D, const std::string& method, bool with_cophenetic = false) {
    int n = D.n; Tree T; T.n = n; T.method = method;
    std::vector<double> d = D.d;
    std::vector<int> id(n), sz(n, 1); std::vector<char> alive(n, 1);
    std::iota(id.begin(), id.end(), 0);
    for (int step = 0; step < n - 1; ++step) {
        int bi = -1, bj = -1; double best = std::numeric_limits<double>::infinity();
        for (int i = 0; i < n; ++i) {
            if (!alive[i]) continue;
            for (int j = i + 1; j < n; ++j)
                if (alive[j] && d[(size_t)i * n + j] < best) { best = d[(size_t)i * n + j]; bi = i; bj = j; }
        }
        int a = std::min(id[bi], id[bj]), b = std::max(id[bi], id[bj]);
        T.m.push_back({a, b, best, sz[bi] + sz[bj]});
        double ni = sz[bi], nj = sz[bj];
        for (int k = 0; k < n; ++k) {
            if (!alive[k] || k == bi || k == bj) continue;
            double dik = d[(size_t)bi * n + k], djk = d[(size_t)bj * n + k], nk = sz[k], nd;
            if (method == "single") nd = std::min(dik, djk);
            else if (method == "complete") nd = std::max(dik, djk);
            else if (method == "average") nd = (ni * dik + nj * djk) / (ni + nj);
            else {  // ward
                double v = ((ni + nk) * dik * dik + (nj + nk) * djk * djk - nk * best * best) / (ni + nj + nk);
                nd = std::sqrt(std::max(v, 0.0));
            }
            d[(size_t)bi * n + k] = d[(size_t)k * n + bi] = nd;
        }
        alive[bj] = 0; sz[bi] += sz[bj]; id[bi] = n + step;
    }
    if (with_cophenetic) {
        std::vector<std::vector<int>> mem(2 * n - 1);
        for (int i = 0; i < n; ++i) mem[i] = {i};
        std::vector<double> coph((size_t)n * n, 0.0);
        for (int s = 0; s < n - 1; ++s) {
            const Merge& mg = T.m[s];
            for (int x : mem[mg.a]) for (int y : mem[mg.b]) coph[(size_t)x * n + y] = coph[(size_t)y * n + x] = mg.h;
            mem[n + s] = mem[mg.a]; mem[n + s].insert(mem[n + s].end(), mem[mg.b].begin(), mem[mg.b].end());
            mem[mg.a].clear(); mem[mg.a].shrink_to_fit(); mem[mg.b].clear(); mem[mg.b].shrink_to_fit();
        }
        double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0; double m = 0;
        for (int i = 0; i < n; ++i) for (int j = i + 1; j < n; ++j) {
            double x = D.at(i, j), y = coph[(size_t)i * n + j]; sx += x; sy += y; sxx += x * x; syy += y * y; sxy += x * y; ++m;
        }
        T.cophenetic = (sxy - sx * sy / m) / std::sqrt((sxx - sx * sx / m) * (syy - sy * sy / m));
    }
    return T;
}

// Cut the tree into k clusters; labels are numbered in order of first appearance.
inline std::vector<int> cut_tree(const Tree& T, int k) {
    int n = T.n; std::vector<int> parent(2 * n - 1);
    std::iota(parent.begin(), parent.end(), 0);
    std::function<int(int)> find = [&](int x) { while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; } return x; };
    for (int s = 0; s < n - k; ++s) { parent[find(T.m[s].a)] = n + s; parent[find(T.m[s].b)] = n + s; }
    std::vector<int> lab(n); std::map<int, int> ids;
    for (int i = 0; i < n; ++i) { int r = find(i); auto it = ids.find(r); if (it == ids.end()) it = ids.insert({r, (int)ids.size()}).first; lab[i] = it->second; }
    return lab;
}
inline double cut_height(const Tree& T, int k) { return 0.5 * (T.m[T.n - k - 1].h + T.m[T.n - k].h); }

// ------------------------------------------------------------------- DBSCAN
inline std::vector<int> dbscan(const DistMat& D, double eps, int min_pts) {
    int n = D.n; std::vector<int> lab(n, -2);
    std::vector<std::vector<int>> nb(n);
    for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j) if (D.at(i, j) <= eps) nb[i].push_back(j);  // includes i itself
    int c = 0;
    for (int i = 0; i < n; ++i) {
        if (lab[i] != -2) continue;
        if ((int)nb[i].size() < min_pts) { lab[i] = -1; continue; }
        lab[i] = c; std::vector<int> seeds(nb[i].begin(), nb[i].end());
        for (size_t s = 0; s < seeds.size(); ++s) {
            int q = seeds[s];
            if (lab[q] == -1) lab[q] = c;
            if (lab[q] != -2) continue;
            lab[q] = c;
            if ((int)nb[q].size() >= min_pts) seeds.insert(seeds.end(), nb[q].begin(), nb[q].end());
        }
        ++c;
    }
    return lab;
}

// sorted distance of every point to its k-th other neighbour
inline Vec k_distances(const DistMat& D, int k) {
    Vec out; out.reserve(D.n);
    for (int i = 0; i < D.n; ++i) {
        Vec row(D.d.begin() + (size_t)i * D.n, D.d.begin() + (size_t)(i + 1) * D.n);
        std::nth_element(row.begin(), row.begin() + k, row.end());
        out.push_back(row[k]);
    }
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace hc
