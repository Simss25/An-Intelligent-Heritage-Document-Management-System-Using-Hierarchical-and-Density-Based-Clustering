// figures.hpp - every figure of the analysis, written as SVG.
#pragma once
#include "context.hpp"

namespace hc {

inline void save_fig(const std::string& dir, const std::string& name, Svg& s, Context& c) {
    write_file(dir + "/" + name, s.str());
    c.figures.push_back(name);
}

inline Axes bounds_axes(double x, double y, double w, double h, const Mat& xy) {
    double x0 = 1e18, x1 = -1e18, y0 = 1e18, y1 = -1e18;
    for (auto& p : xy) { x0 = std::min(x0, p[0]); x1 = std::max(x1, p[0]); y0 = std::min(y0, p[1]); y1 = std::max(y1, p[1]); }
    double px = (x1 - x0) * 0.04 + 1e-9, py = (y1 - y0) * 0.04 + 1e-9;
    return {x, y, w, h, x0 - px, x1 + px, y0 - py, y1 + py};
}
inline void scatter_panel(Svg& s, double x, double y, double w, double h, const Mat& xy, const std::vector<int>& lab, const std::string& title) {
    Axes a = bounds_axes(x, y, w, h, xy);
    s.rect(x, y, w, h, "none", RULE, 0.8);
    s.text(x, y - 8, title, 12, "start", "bold");
    for (size_t i = 0; i < xy.size(); ++i) {
        if (lab[i] < 0) s.cross(a.px(xy[i][0]), a.py(xy[i][1]), 3.4, NOISE_C);
        else s.circle(a.px(xy[i][0]), a.py(xy[i][1]), 3.6, colour(lab[i]));
    }
}

inline void bar_panel(Svg& s, double x, double y, double w, double h, const std::vector<std::string>& names, const Vec& v, const Vec& err,
                      const std::string& title, double ymax) {
    Axes a{x, y, w, h, 0, (double)names.size(), 0, ymax};
    a.frame(s, "", "", true, 0, ymax <= 1.5 ? 1 : 0);
    s.text(x, y - 8, title, 12, "start", "bold");
    double bw = w / names.size();
    for (size_t i = 0; i < names.size(); ++i) {
        double val = isnan_(v[i]) ? 0 : v[i];
        s.rect(x + i * bw + bw * 0.15, a.py(val), bw * 0.7, a.py(0) - a.py(val), colour((int)i));
        if (!err.empty() && !isnan_(err[i])) { double cx = x + (i + 0.5) * bw; s.line(cx, a.py(val - err[i]), cx, a.py(val + err[i])); s.line(cx - 4, a.py(val + err[i]), cx + 4, a.py(val + err[i])); s.line(cx - 4, a.py(val - err[i]), cx + 4, a.py(val - err[i])); }
        s.text(x + (i + 0.5) * bw, y + h + 14, names[i], 9, "end", "normal", INK, -30);
    }
}

inline void dendro_panel(Svg& s, double x, double y, double w, double h, const Tree& T, int k, const std::string& title) {
    int n = T.n; std::vector<int> order; std::vector<int> st = {2 * n - 2};
    while (!st.empty()) {
        int v = st.back(); st.pop_back();
        if (v < n) order.push_back(v);
        else { st.push_back(T.m[v - n].b); st.push_back(T.m[v - n].a); }
    }
    Vec xp(2 * n - 1), hp(2 * n - 1, 0.0); std::vector<int> rep(2 * n - 1);
    for (int i = 0; i < n; ++i) { xp[order[i]] = i; rep[order[i]] = order[i]; }
    for (int m = 0; m < n - 1; ++m) { xp[n + m] = 0.5 * (xp[T.m[m].a] + xp[T.m[m].b]); hp[n + m] = T.m[m].h; rep[n + m] = rep[T.m[m].a]; }
    auto lab = cut_tree(T, k); double cut = cut_height(T, k), hmax = T.m.back().h * 1.05;
    Axes a{x, y, w, h, 0, (double)(n - 1), 0, hmax};
    a.frame(s, "documents (ordered by tree)", "merge distance", true, 0, 1);
    s.text(x, y - 8, title, 12, "start", "bold");
    for (int m = 0; m < n - 1; ++m) {
        int A = T.m[m].a, B = T.m[m].b; double hh = T.m[m].h;
        std::string c = hh <= cut ? colour(lab[rep[A]]) : INK;
        s.line(a.px(xp[A]), a.py(hp[A]), a.px(xp[A]), a.py(hh), c, 0.9);
        s.line(a.px(xp[B]), a.py(hp[B]), a.px(xp[B]), a.py(hh), c, 0.9);
        s.line(a.px(xp[A]), a.py(hh), a.px(xp[B]), a.py(hh), c, 0.9);
    }
    s.line(x, a.py(cut), x + w, a.py(cut), palette()[0], 1.2, "5,4");
}

inline void silhouette_plot(Svg& s, const DistMat& D, const std::vector<int>& lab, const std::string& title) {
    Vec sv = silhouette_values(D, lab);
    std::map<int, Vec> g; for (size_t i = 0; i < lab.size(); ++i) if (lab[i] >= 0 && !isnan_(sv[i])) g[lab[i]].push_back(sv[i]);
    double X = 60, Y = 40, Wd = 500, Ht = 430, tot = 0; for (auto& kv : g) tot += kv.second.size() + 3;
    double bh = std::min(3.0, Ht / std::max(tot, 1.0));
    Axes a{X, Y, Wd, Ht, -0.2, 1.0, 0, 1};
    s.text(X, Y - 14, title, 12, "start", "bold");
    s.line(X, Y + Ht, X + Wd, Y + Ht);
    for (double t : nice_ticks(-0.2, 1.0, 6)) { s.text(a.px(t), Y + Ht + 15, fx(t, 1), 10, "middle"); s.line(a.px(t), Y, a.px(t), Y + Ht, RULE, 0.5); }
    s.text(X + Wd / 2, Y + Ht + 34, "silhouette coefficient", 11, "middle");
    double yy = Y; double sumAll = 0; int cnt = 0;
    for (auto& kv : g) {
        Vec v = kv.second; std::sort(v.begin(), v.end(), std::greater<double>());
        for (double val : v) { double x0 = a.px(std::min(0.0, val)), x1 = a.px(std::max(0.0, val)); s.rect(x0, yy, x1 - x0, bh, colour(kv.first)); yy += bh; sumAll += val; ++cnt; }
        s.text(X - 6, yy - v.size() * bh / 2 + 4, std::to_string(kv.first + 1), 10, "end"); yy += 3 * bh;
    }
    if (cnt) s.line(a.px(sumAll / cnt), Y, a.px(sumAll / cnt), Y + Ht, INK, 1, "4,3");
    s.text(12, Y + Ht / 2, "group", 11, "middle", "normal", INK, -90);
}

inline void terms_panel(Svg& s, const std::string& title, const std::vector<std::pair<int, std::pair<int, std::vector<std::string>>>>& items) {
    s.text(12, 22, title, 13, "start", "bold");
    int cols = 3; double cw = 380, rh = 66;
    for (size_t i = 0; i < items.size(); ++i) {
        double x = 14 + (i % cols) * cw, y = 52 + (i / cols) * rh;
        s.rect(x, y - 14, 6, 44, colour(items[i].first));
        s.text(x + 14, y, "group " + std::to_string(items[i].first + 1) + "  (n=" + std::to_string(items[i].second.first) + ")", 12, "start", "bold");
        std::string l1, l2; const auto& t = items[i].second.second;
        for (size_t j = 0; j < t.size() && j < 6; ++j) (j < 3 ? l1 : l2) += (j % 3 ? ", " : "") + t[j];
        s.text(x + 14, y + 17, l1, 11); s.text(x + 14, y + 32, l2, 11);
    }
}

inline void contingency_fig(Svg& s, const std::vector<int>& lab, const std::vector<std::string>& ref, const std::string& title) {
    std::set<std::string> cs(ref.begin(), ref.end()); std::vector<std::string> cats(cs.begin(), cs.end());
    std::set<int> rs(lab.begin(), lab.end()); std::vector<int> rows(rs.begin(), rs.end());
    std::vector<std::vector<int>> t(rows.size(), std::vector<int>(cats.size(), 0)); int mx = 1;
    for (size_t i = 0; i < lab.size(); ++i) {
        int r = (int)(std::find(rows.begin(), rows.end(), lab[i]) - rows.begin()), c = (int)(std::find(cats.begin(), cats.end(), ref[i]) - cats.begin());
        mx = std::max(mx, ++t[r][c]);
    }
    double X = 70, Y = 120, cw = 62, ch = 22;
    s.text(12, 22, title, 13, "start", "bold");
    for (size_t c = 0; c < cats.size(); ++c) s.text(X + c * cw + cw / 2, Y - 8, cats[c], 9, "start", "normal", INK, -35);
    for (size_t r = 0; r < rows.size(); ++r) {
        s.text(X - 8, Y + r * ch + 15, rows[r] < 0 ? "noise" : std::to_string(rows[r] + 1), 10, "end");
        for (size_t c = 0; c < cats.size(); ++c) {
            s.rect(X + c * cw, Y + r * ch, cw - 1, ch - 1, heat_colour(t[r][c] / (double)mx * 0.9));
            if (t[r][c]) s.text(X + c * cw + cw / 2, Y + r * ch + 15, std::to_string(t[r][c]), 10, "middle");
        }
    }
    s.text(14, Y + rows.size() * ch / 2, "discovered group", 11, "middle", "normal", INK, -90);
}

inline void make_figures(Context& c, const std::string& dir) {
    const Config& C = cfg(); c.figures.clear();
    int n = c.D.n;
    std::vector<std::string> L = C.linkages;

    { // 01 missing values
        Svg s(640, 320); s.text(14, 24, "Missing metadata before preprocessing", 13, "start", "bold");
        double X = 130, w = 440; int i = 0; double mx = 1;
        for (auto& r : c.F.notes.missing) mx = std::max(mx, r.percent);
        for (auto& r : c.F.notes.missing) {
            double y = 50 + i * 40; s.text(X - 8, y + 18, r.field, 12, "end");
            s.rect(X, y, w * r.percent / mx, 26, palette()[0]); s.text(X + w * r.percent / mx + 6, y + 18, fx(r.percent, 1) + "%", 11); ++i;
        }
        save_fig(dir, "01_missing_values.svg", s, c);
    }
    { // 02 scree
        Svg s(640, 340); int m = std::min<int>(15, c.pca_ratio.size());
        double cum = 0, cmax = 0; for (int i = 0; i < m; ++i) cmax += c.pca_ratio[i];
        Axes a{70, 50, 540, 240, 0, (double)m, 0, std::max(cmax, 0.1) * 1.05};
        a.frame(s, "principal component", "explained variance ratio", true, 0, 2);
        s.text(14, 24, "PCA scree: how much of the structure two axes can show", 13, "start", "bold");
        std::vector<std::pair<double, double>> pts; double bw = a.w / m;
        for (int i = 0; i < m; ++i) {
            cum += c.pca_ratio[i];
            s.rect(a.x + i * bw + 2, a.py(c.pca_ratio[i]), bw - 4, a.py(0) - a.py(c.pca_ratio[i]), palette()[1]);
            pts.push_back({a.x + (i + 0.5) * bw, a.py(cum)}); s.text(a.x + (i + 0.5) * bw, a.y + a.h + 15, std::to_string(i + 1), 10, "middle");
        }
        s.polyline(pts, palette()[0], 2); for (auto& p : pts) s.circle(p.first, p.second, 3, palette()[0]);
        s.text(a.x + a.w - 10, a.y + 14, "line = cumulative", 10, "end");
        save_fig(dir, "02_pca_scree.svg", s, c);
    }
    { // 03 dendrograms
        Svg s(1240, 760); s.text(14, 24, "Dendrograms (dashed line = cut at best k)", 14, "start", "bold");
        for (int i = 0; i < 4; ++i) {
            auto& r = c.H[L[i]];
            dendro_panel(s, 70 + (i % 2) * 610, 70 + (i / 2) * 350, 520, 250, r.tree, r.best_k, L[i] + " linkage (k=" + std::to_string(r.best_k) + ")");
        }
        save_fig(dir, "03_dendrograms.svg", s, c);
    }
    { // 04 silhouette vs k
        Svg s(720, 400); Axes a{70, 50, 560, 290, (double)C.k_min, (double)C.k_max, 1e9, -1e9};
        for (auto& l : L) for (double v : c.H[l].sil_by_k) if (!isnan_(v)) { a.y0 = std::min(a.y0, v); a.y1 = std::max(a.y1, v); }
        a.y0 = std::min(a.y0, 0.0); a.y1 *= 1.08;
        a.frame(s, "number of clusters k", "silhouette", true, 0, 2);
        s.text(14, 24, "Silhouette against k, per linkage (ring = best k)", 13, "start", "bold");
        for (size_t i = 0; i < L.size(); ++i) {
            std::vector<std::pair<double, double>> pts;
            for (int k = C.k_min; k <= C.k_max; ++k) { double v = c.H[L[i]].sil_by_k[k - C.k_min]; if (!isnan_(v)) pts.push_back({a.px(k), a.py(v)}); }
            s.polyline(pts, palette()[i], 2); for (auto& p : pts) s.circle(p.first, p.second, 3, palette()[i]);
            auto& r = c.H[L[i]]; s.raw("<circle cx='" + f1(a.px(r.best_k)) + "' cy='" + f1(a.py(r.sil)) + "' r='8' fill='none' stroke='" + palette()[i] + "' stroke-width='1.6'/>");
            s.rect(650, 70 + i * 20, 12, 12, palette()[i]); s.text(668, 81 + i * 20, L[i], 11);
        }
        save_fig(dir, "04_silhouette_vs_k.svg", s, c);
    }
    { // 05 pca hierarchical
        Svg s(1280, 340); s.text(14, 22, "PCA projection coloured by hierarchical cluster", 13, "start", "bold");
        for (int i = 0; i < 4; ++i) { auto& r = c.H[L[i]]; scatter_panel(s, 14 + i * 316, 56, 296, 260, c.xy, r.labels, L[i] + ": k=" + std::to_string(r.best_k) + ", sil=" + fx(r.sil, 2)); }
        save_fig(dir, "05_pca_hierarchical.svg", s, c);
    }
    { // 06 k-distance
        Svg s(720, 400); Axes a{70, 50, 560, 290, 0, (double)n, 0, 0};
        std::vector<Vec> ks; for (int k : C.min_pts_grid) { ks.push_back(k_distances(c.D, k)); a.y1 = std::max(a.y1, ks.back().back()); }
        a.y1 *= 1.05; a.frame(s, "documents sorted by distance", "distance to MinPts-th neighbour", true, 0, 2);
        s.text(14, 24, "k-distance curves: the knee suggests a good epsilon", 13, "start", "bold");
        for (size_t i = 0; i < ks.size(); ++i) {
            std::vector<std::pair<double, double>> pts; for (int j = 0; j < n; ++j) pts.push_back({a.px(j), a.py(ks[i][j])});
            s.polyline(pts, palette()[i], 2); s.rect(70, 0, 0, 0, "none");
            s.rect(90 + i * 110, 354 + 22, 12, 12, palette()[i]); s.text(108 + i * 110, 365 + 22, "MinPts=" + std::to_string(C.min_pts_grid[i]), 11);
        }
        save_fig(dir, "06_k_distance.svg", s, c);
    }
    { // 07 DBSCAN sensitivity heatmaps
        int R = (int)C.min_pts_grid.size(), Cn = C.eps_points; double cw = 21, ch = 36, pw = Cn * cw;
        Svg s(1480, 280); s.text(14, 22, "DBSCAN parameter sensitivity", 13, "start", "bold");
        const char* names[3] = {"Clusters found", "Noise fraction", "Silhouette (non-noise)"};
        for (int p = 0; p < 3; ++p) {
            double X = 60 + p * (pw + 90), Y = 70; s.text(X, Y - 10, names[p], 12, "start", "bold");
            double lo = 1e9, hi = -1e9;
            auto val = [&](const GridRow& g) { return p == 0 ? (double)g.n_clusters : p == 1 ? g.noise : g.sil; };
            for (auto& g : c.grid) if (!isnan_(val(g))) { lo = std::min(lo, val(g)); hi = std::max(hi, val(g)); }
            for (int r = 0; r < R; ++r) {
                s.text(X - 6, Y + r * ch + 22, std::to_string(C.min_pts_grid[r]), 10, "end");
                for (int e = 0; e < Cn; ++e) {
                    const GridRow& g = c.grid[r * Cn + e]; double v = val(g);
                    s.rect(X + e * cw, Y + r * ch, cw - 1, ch - 1, isnan_(v) ? "#e9e1cf" : heat_colour((v - lo) / (hi - lo + 1e-12)));
                    if (p == 0) s.text(X + e * cw + cw / 2, Y + r * ch + 22, std::to_string(g.n_clusters), 8, "middle");
                }
            }
            for (int e = 0; e < Cn; e += 2) s.text(X + e * cw + cw / 2, Y + R * ch + 12, fx(c.eps_values[e], 2), 8, "start", "normal", INK, 60);
            s.text(X + pw / 2, Y + R * ch + 52, "epsilon", 11, "middle"); s.text(X - 34, Y + R * ch / 2, "MinPts", 11, "middle", "normal", INK, -90);
            s.text(X + pw + 6, Y + 12, fx(hi, 2), 9); s.text(X + pw + 6, Y + R * ch, fx(lo, 2), 9);
        }
        save_fig(dir, "07_dbscan_sensitivity.svg", s, c);
    }
    { // 08 DBSCAN at three epsilons
        Svg s(1000, 340); s.text(14, 22, "DBSCAN at different epsilon (grey x = noise)", 13, "start", "bold");
        double fs[3] = {0.8, 1.0, 1.25};
        for (int i = 0; i < 3; ++i) {
            auto lab = i == 1 ? c.db_labels : dbscan(c.D, c.best_db.eps * fs[i], c.best_db.min_pts);
            int nc = 0, nz = 0; for (int l : lab) { nc = std::max(nc, l + 1); nz += l == -1; }
            scatter_panel(s, 14 + i * 330, 56, 310, 260, c.xy, lab, "eps=" + fx(c.best_db.eps * fs[i], 2) + ": " + std::to_string(nc) + " clusters, " + fx(100.0 * nz / n, 0) + "% noise");
        }
        save_fig(dir, "08_pca_dbscan.svg", s, c);
    }
    { Svg s(620, 520); silhouette_plot(s, c.D, c.H[c.best_h].labels, "Silhouette plot: hierarchical (" + c.best_h + ")"); save_fig(dir, "09_silhouette_hier.svg", s, c); }
    { Svg s(620, 520); silhouette_plot(s, c.D, c.db_labels, "Silhouette plot: DBSCAN (noise excluded)"); save_fig(dir, "10_silhouette_dbscan.svg", s, c); }
    { // 11 stability
        Svg s(760, 380); std::vector<std::string> nm; Vec mu, sd;
        for (auto& l : L) { nm.push_back("hier-" + l); mu.push_back(c.stab["hier-" + l].mean); sd.push_back(c.stab["hier-" + l].sd); }
        nm.push_back("dbscan"); mu.push_back(c.stab["dbscan"].mean); sd.push_back(c.stab["dbscan"].sd);
        bar_panel(s, 70, 50, 640, 240, nm, mu, sd, "Stability under " + fx(C.subsample_frac * 100, 0) + "% subsampling (" + std::to_string(C.n_subsamples) + " runs), ARI vs full-data labels", 1.05);
        save_fig(dir, "11_stability.svg", s, c);
    }
    { // 12 comparison
        Svg s(1260, 340); s.text(14, 22, "Method comparison on the same feature space", 13, "start", "bold");
        std::vector<std::string> nm; Vec a1, a2, a3; for (auto& r : c.summary) { nm.push_back(r.method); a1.push_back(r.sil); a2.push_back(r.stab); a3.push_back(r.smallest); }
        double m3 = *std::max_element(a3.begin(), a3.end());
        bar_panel(s, 50, 60, 340, 220, nm, a1, {}, "Silhouette", 1.0);
        bar_panel(s, 450, 60, 340, 220, nm, a2, {}, "Stability (ARI)", 1.0);
        bar_panel(s, 850, 60, 340, 220, nm, a3, {}, "Smallest cluster size", m3 * 1.1 + 1);
        save_fig(dir, "12_method_comparison.svg", s, c);
    }
    if (c.has_ref) {
        { Svg s(1000, 420); contingency_fig(s, c.H[c.best_h].labels, c.ref, "Hierarchical (" + c.best_h + ") groups vs catalogue subject"); save_fig(dir, "13_contingency_hier.svg", s, c); }
        { Svg s(1000, 420); contingency_fig(s, c.db_labels, c.ref, "DBSCAN groups vs catalogue subject"); save_fig(dir, "14_contingency_dbscan.svg", s, c); }
    }
    auto items = [&](const std::vector<int>& lab) {
        auto tt = top_terms(c.F, lab); std::map<int, int> sz; for (int l : lab) sz[l]++;
        std::vector<std::pair<int, std::pair<int, std::vector<std::string>>>> it;
        for (auto& kv : tt) if (kv.first >= 0 && it.size() < 12) it.push_back({kv.first, {sz[kv.first], kv.second}});
        return it;
    };
    { auto it = items(c.H[c.best_h].labels); Svg s(1180, 60 + 66 * ((it.size() + 2) / 3)); terms_panel(s, "Distinctive terms per hierarchical group (" + c.best_h + ")", it); save_fig(dir, "15_terms_hier.svg", s, c); }
    { auto it = items(c.db_labels); Svg s(1180, 60 + 66 * ((it.size() + 2) / 3)); terms_panel(s, "Distinctive terms per DBSCAN group", it); save_fig(dir, "16_terms_dbscan.svg", s, c); }
}

}  // namespace hc
