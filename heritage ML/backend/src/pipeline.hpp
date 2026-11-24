// pipeline.hpp - end-to-end analysis, JSON for the API, auto-written report.
#pragma once
#include <memory>
#include "figures.hpp"

namespace hc {

inline std::string cluster_describe_json(const Context& c, const std::vector<int>& lab, bool with_ids, int max_titles) {
    Vec sv = silhouette_values(c.D, lab);
    auto tt = top_terms(c.F, lab);
    std::map<int, std::vector<int>> g; for (size_t i = 0; i < lab.size(); ++i) g[lab[i]].push_back((int)i);
    std::vector<std::string> items;
    for (auto& kv : g) {
        if (kv.first == -1 && with_ids) continue;  // noise is reported separately for the cluster cards
        double ms = 0; int cnt = 0; for (int i : kv.second) if (!isnan_(sv[i])) { ms += sv[i]; ++cnt; }
        std::map<std::string, int> langs; Vec years; std::vector<std::string> titles, ids;
        for (int i : kv.second) {
            langs[c.docs[i].has_lang ? c.docs[i].lang : "Unknown"]++; years.push_back(c.docs[i].year);
            if ((int)titles.size() < max_titles) { titles.push_back(c.docs[i].title); }
            ids.push_back(c.docs[i].id);
        }
        std::string ml; int mc = 0; for (auto& l : langs) if (l.second > mc) { mc = l.second; ml = l.first; }
        J::KV o = {{"id", std::to_string(kv.first)}, {"size", std::to_string(kv.second.size())}, {"top_terms", J::strs(tt[kv.first])},
                   {"mean_silhouette", J::num(cnt ? ms / cnt : NaN)}, {"titles", J::strs(titles)}};
        if (with_ids) { o.push_back({"main_language", J::str(ml)}); o.push_back({"language_share", J::num((double)mc / kv.second.size())});
                        o.push_back({"median_year", J::num(median(years))}); o.push_back({"doc_ids", J::strs(ids)}); }
        items.push_back(J::obj(o));
    }
    return "[" + [&] { std::string s; for (size_t i = 0; i < items.size(); ++i) s += (i ? "," : "") + items[i]; return s; }() + "]";
}

inline GridRow pick_dbscan(const std::vector<GridRow>& rows) {
    const Config& C = cfg(); const GridRow* best = nullptr;
    for (auto& r : rows)
        if (!isnan_(r.sil) && r.n_clusters >= C.min_clusters && r.n_clusters <= C.max_clusters && r.noise <= C.max_noise && (!best || r.sil > best->sil)) best = &r;
    if (!best) for (auto& r : rows) if (!isnan_(r.sil) && (!best || r.sil > best->sil)) best = &r;
    return best ? *best : rows[0];
}

inline std::string fnum(double v, int d = 2) { return isnan_(v) ? "n/a" : fx(v, d); }

inline std::string make_report(const Context& c) {
    const Config& C = cfg(); std::ostringstream r; const Notes& N = c.F.notes; const GridRow& b = c.best_db;
    r << "# Heritage Document Topic Grouping: Hierarchical and Density-Based Clustering (C++ implementation)\n\n"
      << "Corpus file: `" << c.csv_name << "` (" << c.D.n << " documents). Random seed: " << C.seed << ". Figures are in `outputs/figures/`; run `heritage analyze` to regenerate this file.\n\n"
      << "## 1. Problem\n\nDigital libraries such as the National Digital Library of India hold millions of scanned books, gazetteers, manuscripts and pamphlets whose catalogue records are incomplete, inconsistently labelled and in many languages. "
         "This project groups scans into topical shelves **without using subject labels**, and flags documents that fit no group so a human cataloguer can review them.\n\n"
      << "## 2. Data preparation\n\n| Field | Missing | Handling |\n|---|---|---|\n";
    std::map<std::string, std::string> hand = {{"title", "required, never imputed"}, {"description", "title text only; flag kept"}, {"language", "category `Unknown`, then one-hot"},
        {"year", "median impute + missing flag + standardise"}, {"pages", "median impute, log, standardise"}, {"collection", "category `Unknown`, then one-hot"}};
    for (auto& m : N.missing) r << "| " << m.field << " | " << m.missing << " (" << fx(m.percent, 1) << "%) | " << hand[m.field] << " |\n";
    r << "\nText is cleaned (lower-case, OCR digit/letter swaps repaired, punctuation removed), converted to TF-IDF (" << N.n_terms << " terms, 1-2 grams, sublinear tf, smooth idf) and compressed with LSA to "
      << N.lsa_components << " dimensions (" << fx(N.lsa_share * 100, 0) << "% of TF-IDF signal retained). Year, log-pages and log-length are standardised; language and collection are one-hot encoded. Block weights: text="
      << C.w_text << ", numeric=" << C.w_num << ", categorical=" << C.w_cat << ". Final matrix: " << N.total_dims << " dimensions.\n\n"
      << "## 3. Hierarchical clustering\n\n| Linkage | Best k | Silhouette | Cophenetic corr. | Smallest / largest | Stability (ARI) |\n|---|---|---|---|---|---|\n";
    for (auto& l : C.linkages) {
        auto& h = c.H.at(l); auto& s = c.stab.at("hier-" + l);
        r << "| " << l << " | " << h.best_k << " | " << fnum(h.sil) << " | " << fnum(h.tree.cophenetic) << " | " << *std::min_element(h.sizes.begin(), h.sizes.end()) << " / "
          << *std::max_element(h.sizes.begin(), h.sizes.end()) << " | " << fnum(s.mean) << " ± " << fnum(s.sd) << " |\n";
    }
    r << "\nHighest silhouette: **" << c.best_h << "**. Single linkage tends to chain (tiny clusters plus one giant one); ward and average give more balanced groups. The result is **linkage-sensitive** and should not be reported as one 'true' structure.\n\n"
      << "## 4. DBSCAN\n\nGrid: MinPts in {";
    for (size_t i = 0; i < C.min_pts_grid.size(); ++i) r << (i ? ", " : "") << C.min_pts_grid[i];
    r << "}, " << C.eps_points << " epsilon values per MinPts from the k-distance curves. Selection rule: " << C.min_clusters << "-" << C.max_clusters << " clusters, noise <= " << fx(C.max_noise * 100, 0)
      << "%, then highest silhouette (noise excluded).\n\nSelected: eps = " << fx(b.eps, 3) << ", MinPts = " << b.min_pts << " -> " << b.n_clusters << " clusters, " << fx(b.noise * 100, 0) << "% noise, silhouette " << fnum(b.sil) << ".\n\n"
      << "| eps x | Clusters | Noise | ARI vs selected |\n|---|---|---|---|\n";
    for (auto& p : c.perturb) r << "| " << p.factor << " | " << p.n_clusters << " | " << fx(p.noise * 100, 0) << "% | " << fnum(p.ari) << " |\n";
    r << "\nSmall changes of epsilon move many documents between 'cluster' and 'noise'. Silhouette on non-noise points rewards settings that discard awkward documents, so coverage (1 - noise) must be read with it.\n\n"
      << "## 5. Comparison\n\n| Method | Clusters | Noise | Silhouette | Stability |\n|---|---|---|---|---|\n";
    for (auto& s : c.summary) r << "| " << s.method << " | " << s.clusters << " | " << fx(s.noise * 100, 0) << "% | " << fnum(s.sil) << " | " << fnum(s.stab) << " |\n";
    r << "\nARI between best hierarchical labels and DBSCAN on documents DBSCAN placed: " << fnum(c.agreement) << ". This ignores the noise documents, and DBSCAN's dense cores are subsets of larger hierarchical groups.\n\n";
    if (!c.external.empty()) {
        r << "External sanity check against catalogue subjects (not used for clustering; the labels are themselves imperfect):\n\n| Method | ARI | NMI |\n|---|---|---|\n";
        for (auto& e : c.external) r << "| " << e.method << " | " << fnum(e.ari) << " | " << fnum(e.nmi) << " |\n";
        r << "\n";
    }
    r << "## 6. Noise observations\n\nDBSCAN labelled " << c.noise_idx.size() << " documents as noise. Examples:\n\n";
    for (size_t i = 0; i < c.noise_idx.size() && i < 8; ++i) { const Doc& d = c.docs[c.noise_idx[i]]; r << "- **" << d.title << "**: " << (d.has_desc ? utf8_cut(d.desc, 90) : "(no text)") << "\n"; }
    r << "\nTypical noise: blank leaves, ledger or index fragments, advertisements, badly damaged OCR and mixed-subject volumes. Noise is treated as a **review queue**, not an error.\n\n"
      << "## 7. Limitations\n\n"
         "- **Sample**: a small sample is not the library; cluster counts and shapes change with more documents.\n"
         "- **Excerpts, not books**: titles and short OCR excerpts carry thin signal and are dominated by catalogue wording.\n"
         "- **OCR quality**: damaged text clusters with other damaged text, not by topic.\n"
         "- **Language**: TF-IDF with English stop-words serves other languages poorly; non-English records may group by transliteration habit.\n"
         "- **Silhouette is not truth**: it favours compact convex groups and cannot judge historical meaning.\n"
         "- **Parameters**: linkage, k, epsilon, MinPts and feature weights all change the result.\n"
         "- **Distance concentration** in high dimensions hurts density methods; LSA reduces but does not remove it.\n\n"
      << "## 8. Bias and ethical considerations\n\n"
         "- **Whose heritage is visible?** Digitisation and cataloguing priorities favour some languages, regions and communities; clusters reproduce that skew.\n"
         "- **Colonial vocabulary**: administrative reports describe people in the administrators' terms; surfacing groups uncritically amplifies that framing.\n"
         "- **Minority-language invisibility**: small collections look like noise to density methods, so noise must go to human review, never to deletion.\n"
         "- **Metadata features**: language and year can pull documents together for non-topical reasons; their weights are documented and low.\n"
         "- **Automation boundary**: output is a suggestion for librarians; no document should be hidden or ranked lower because of its label.\n"
         "- **Copyright and access**: use only openly licensed scans and keep links to the original records.\n\n"
      << "## 9. Conclusions\n\nHierarchical clustering gives a complete grouping with nested structure but depends on linkage. DBSCAN found " << b.n_clusters << " dense groups and isolated "
      << fx(b.noise * 100, 0) << "% of documents as noise, useful for quality control but very sensitive to epsilon. Using both gives stable shelves where methods agree and a review queue where they do not.\n";
    return r.str();
}

inline std::shared_ptr<Context> analyze(const std::string& csv_path, const std::string& out_dir, bool verbose = true) {
    const Config& C = cfg(); auto log = [&](const std::string& s) { if (verbose) std::cout << s << std::endl; };
    auto cp = std::make_shared<Context>(); Context& c = *cp;
    { size_t p = csv_path.find_last_of("/\\"); c.csv_name = p == std::string::npos ? csv_path : csv_path.substr(p + 1); }
    c.docs = load_corpus(csv_path); c.F = build_features(c.docs); c.D = euclid(c.F.X);
    int n = c.D.n; const Features& F = c.F;
    log("[1/6] features: " + std::to_string(n) + " documents x " + std::to_string(F.notes.total_dims) + " dims (" + std::to_string(F.notes.n_terms) + " terms)");

    // ---- PCA (display only) ----
    int d = (int)F.X[0].size(); Vec mu(d, 0.0);
    for (auto& r : F.X) for (int j = 0; j < d; ++j) mu[j] += r[j] / n;
    Mat cov(d, Vec(d, 0.0));
    for (auto& r : F.X) for (int a = 0; a < d; ++a) for (int b = a; b < d; ++b) cov[a][b] += (r[a] - mu[a]) * (r[b] - mu[b]) / (n - 1);
    for (int a = 0; a < d; ++a) for (int b = 0; b < a; ++b) cov[a][b] = cov[b][a];
    Vec ev; Mat EV; jacobi_eigen(cov, ev, EV);
    double tot = 0; for (double v : ev) tot += std::max(v, 0.0);
    for (double v : ev) c.pca_ratio.push_back(std::max(v, 0.0) / tot);
    c.xy.assign(n, Vec(2, 0.0));
    for (int comp = 0; comp < 2; ++comp) {
        int big = 0; for (int j = 0; j < d; ++j) if (std::fabs(EV[j][comp]) > std::fabs(EV[big][comp])) big = j;
        double sg = EV[big][comp] < 0 ? -1 : 1;
        for (int i = 0; i < n; ++i) { double s = 0; for (int j = 0; j < d; ++j) s += (F.X[i][j] - mu[j]) * EV[j][comp]; c.xy[i][comp] = sg * s; }
    }

    // ---- hierarchical ----
    std::string line = "[2/6] hierarchical:";
    for (auto& l : C.linkages) {
        HierResult h; h.tree = agglomerate(c.D, l, true);
        double best = -1e9; h.best_k = 0;
        for (int k = C.k_min; k <= C.k_max; ++k) {
            double s = silhouette_score(c.D, cut_tree(h.tree, k)); h.sil_by_k.push_back(s);
            if (!isnan_(s) && s > best) { best = s; h.best_k = k; }
        }
        h.sil = best; h.labels = cut_tree(h.tree, h.best_k); h.sizes.assign(h.best_k, 0); for (int x : h.labels) h.sizes[x]++;
        line += " " + l + " k=" + std::to_string(h.best_k) + " sil=" + fx(h.sil, 2) + ";";
        c.H[l] = std::move(h);
    }
    log(line);
    c.best_h = C.linkages[0];
    for (auto& l : C.linkages) if (c.H[l].sil > c.H[c.best_h].sil) c.best_h = l;

    // ---- DBSCAN grid ----
    double lo = 1e18, hi = -1e18;
    for (int k : C.min_pts_grid) { Vec kd = k_distances(c.D, k); lo = std::min(lo, percentile(kd, 10)); hi = std::max(hi, percentile(kd, 95)); }
    for (int i = 0; i < C.eps_points; ++i) c.eps_values.push_back(std::round((lo + (hi - lo) * i / (C.eps_points - 1)) * 1e4) / 1e4);
    for (int mp : C.min_pts_grid) for (double eps : c.eps_values) {
        auto lab = dbscan(c.D, eps, mp); int nc = 0, nz = 0; for (int x : lab) { nc = std::max(nc, x + 1); nz += x == -1; }
        c.grid.push_back({eps, mp, nc, (double)nz / n, silhouette_score(c.D, lab)});
    }
    c.best_db = pick_dbscan(c.grid); c.db_labels = dbscan(c.D, c.best_db.eps, c.best_db.min_pts);
    log("[3/6] DBSCAN best: eps=" + fx(c.best_db.eps, 3) + " MinPts=" + std::to_string(c.best_db.min_pts) + " -> " + std::to_string(c.best_db.n_clusters) + " clusters, " + fx(c.best_db.noise * 100, 0) + "% noise, sil=" + fnum(c.best_db.sil));

    // ---- stability ----
    for (auto& l : C.linkages) { int k = c.H[l].best_k; c.stab["hier-" + l] = stability(c.D, c.H[l].labels, [l, k](const DistMat& s) { return cut_tree(agglomerate(s, l), k); }); }
    double eps = c.best_db.eps; int mp = c.best_db.min_pts;
    c.stab["dbscan"] = stability(c.D, c.db_labels, [eps, mp](const DistMat& s) { return dbscan(s, eps, mp); });
    for (double f : {0.8, 0.9, 1.1, 1.25}) {
        auto lab = dbscan(c.D, eps * f, mp); int nc = 0, nz = 0; for (int x : lab) { nc = std::max(nc, x + 1); nz += x == -1; }
        c.perturb.push_back({f, ari(c.db_labels, lab), nc, (double)nz / n});
    }
    log("[4/6] stability computed (" + std::to_string(C.n_subsamples) + " subsamples per method)");

    // ---- summary / external validation ----
    for (auto& l : C.linkages) { auto& h = c.H[l]; c.summary.push_back({"hier-" + l, h.best_k, 0.0, h.sil, c.stab["hier-" + l].mean, *std::min_element(h.sizes.begin(), h.sizes.end()), *std::max_element(h.sizes.begin(), h.sizes.end()), h.tree.cophenetic}); }
    { std::map<int, int> sz; for (int x : c.db_labels) if (x >= 0) sz[x]++; int mn = 1 << 30, mx = 0; for (auto& kv : sz) { mn = std::min(mn, kv.second); mx = std::max(mx, kv.second); } if (sz.empty()) mn = 0;
      c.summary.push_back({"dbscan", c.best_db.n_clusters, c.best_db.noise, c.best_db.sil, c.stab["dbscan"].mean, mn, mx, NaN}); }
    for (auto& dct : c.docs) { c.has_ref |= dct.has_subject; c.ref.push_back(dct.has_subject ? dct.subject : "(no subject)"); }
    std::vector<int> keep; for (int i = 0; i < n; ++i) if (c.db_labels[i] != -1) keep.push_back(i);
    auto sub = [&](const std::vector<int>& v) { std::vector<int> o; for (int i : keep) o.push_back(v[i]); return o; };
    std::vector<std::string> refk; for (int i : keep) refk.push_back(c.ref[i]);
    c.agreement = keep.size() > 2 ? ari(sub(c.H[c.best_h].labels), sub(c.db_labels)) : NaN;
    if (c.has_ref) {
        auto r = encode(c.ref);
        for (auto& l : C.linkages) c.external.push_back({"hier-" + l, ari(r, c.H[l].labels), nmi(r, c.H[l].labels), 1.0});
        auto rk = encode(refk); auto dk = sub(c.db_labels);
        if (!keep.empty()) c.external.push_back({"dbscan", ari(rk, dk), nmi(rk, dk), (double)keep.size() / n});
    }
    for (int i = 0; i < n; ++i) if (c.db_labels[i] == -1) c.noise_idx.push_back(i);

    // ---- figures + report ----
    make_figures(c, out_dir + "/figures");
    log("[5/6] wrote " + std::to_string(c.figures.size()) + " figures to " + out_dir + "/figures");

    // ---- JSON ----
    Vec shv = silhouette_values(c.D, c.H[c.best_h].labels);
    std::vector<std::string> docs;
    for (int i = 0; i < n; ++i) {
        const Doc& dd = c.docs[i]; J::KV hier; for (auto& l : C.linkages) hier.push_back({l, std::to_string(c.H[l].labels[i])});
        docs.push_back(J::obj({{"doc_id", J::str(dd.id)}, {"title", J::str(dd.title)}, {"description", dd.has_desc ? J::str(dd.desc) : "null"},
            {"language", dd.has_lang ? J::str(dd.lang) : "null"}, {"year", J::num(dd.year)}, {"pages", J::num(dd.pages)},
            {"collection", dd.has_coll ? J::str(dd.collection) : "null"}, {"reference_topic", dd.has_subject ? J::str(dd.subject) : "null"},
            {"x", J::num(c.xy[i][0])}, {"y", J::num(c.xy[i][1])}, {"hier", J::obj(hier)}, {"dbscan", std::to_string(c.db_labels[i])}, {"sil_hier", J::num(shv[i])}}));
    }
    { std::string s = "["; for (size_t i = 0; i < docs.size(); ++i) s += (i ? "," : "") + docs[i]; c.docs_json = s + "]"; }

    J::KV miss; std::vector<std::string> mrows;
    for (auto& m : F.notes.missing) mrows.push_back(J::obj({{"field", J::str(m.field)}, {"missing", std::to_string(m.missing)}, {"percent", J::num(m.percent)}}));
    std::string missing = "["; for (size_t i = 0; i < mrows.size(); ++i) missing += (i ? "," : "") + mrows[i]; missing += "]";
    J::KV hjson;
    for (auto& l : C.linkages) {
        auto& h = c.H[l]; J::KV sk; for (int k = C.k_min; k <= C.k_max; ++k) sk.push_back({std::to_string(k), J::num(h.sil_by_k[k - C.k_min])});
        hjson.push_back({l, J::obj({{"cophenetic", J::num(h.tree.cophenetic)}, {"silhouette_by_k", J::obj(sk)}, {"best_k", std::to_string(h.best_k)}, {"silhouette", J::num(h.sil)},
            {"sizes", J::ints(h.sizes)}, {"smallest", std::to_string(*std::min_element(h.sizes.begin(), h.sizes.end()))}, {"largest", std::to_string(*std::max_element(h.sizes.begin(), h.sizes.end()))}})});
    }
    auto grow = [](const GridRow& g) { return J::obj({{"eps", J::num(g.eps)}, {"min_pts", std::to_string(g.min_pts)}, {"n_clusters", std::to_string(g.n_clusters)}, {"noise_frac", J::num(g.noise)}, {"silhouette", J::num(g.sil)}}); };
    J::KV pj; for (auto& p : c.perturb) pj.push_back({J::num(p.factor), J::obj({{"ari", J::num(p.ari)}, {"n_clusters", std::to_string(p.n_clusters)}, {"noise_frac", J::num(p.noise)}})});
    J::KV sj; for (auto& kv : c.stab) sj.push_back({kv.first, J::obj({{"mean", J::num(kv.second.mean)}, {"sd", J::num(kv.second.sd)}})});
    std::vector<std::string> sumj; for (auto& s : c.summary) sumj.push_back(J::obj({{"method", J::str(s.method)}, {"clusters", std::to_string(s.clusters)}, {"noise", J::num(s.noise)}, {"silhouette", J::num(s.sil)}, {"stability", J::num(s.stab)}, {"smallest", std::to_string(s.smallest)}, {"largest", std::to_string(s.largest)}, {"cophenetic", J::num(s.coph)}}));
    J::KV ext; for (auto& e : c.external) ext.push_back({e.method, J::obj({{"ari", J::num(e.ari)}, {"nmi", J::num(e.nmi)}, {"coverage", J::num(e.coverage)}})});
    std::vector<std::string> noisej;
    for (int i : c.noise_idx) { const Doc& dd = c.docs[i]; noisej.push_back(J::obj({{"doc_id", J::str(dd.id)}, {"title", J::str(dd.title)}, {"text", J::str(utf8_cut(dd.desc, 160))}, {"language", dd.has_lang ? J::str(dd.lang) : "null"}, {"year", J::num(dd.year)}, {"reference_topic", dd.has_subject ? J::str(dd.subject) : "null"}, {"missing_description", J::boolean(!dd.has_desc)}})); }
    auto joinv = [](const std::vector<std::string>& v) { std::string s = "["; for (size_t i = 0; i < v.size(); ++i) s += (i ? "," : "") + v[i]; return s + "]"; };

    c.analysis_json = J::obj({
        {"meta", J::obj({{"csv", J::str(c.csv_name)}, {"n_docs", std::to_string(n)}, {"seed", std::to_string(C.seed)}, {"has_reference", J::boolean(c.has_ref)}, {"engine", J::str("c++17")}})},
        {"features", J::obj({{"n_docs", std::to_string(n)}, {"n_terms", std::to_string(F.notes.n_terms)}, {"lsa_components", std::to_string(F.notes.lsa_components)}, {"lsa_variance", J::num(F.notes.lsa_share)},
                             {"total_dims", std::to_string(F.notes.total_dims)}, {"missing", missing}})},
        {"pca_variance", J::nums(Vec(c.pca_ratio.begin(), c.pca_ratio.begin() + 2))},
        {"hierarchical", J::obj(hjson)},
        {"dbscan", J::obj({{"best", grow(c.best_db)}, {"eps_values", J::nums(c.eps_values)}, {"grid", joinv([&] { std::vector<std::string> v; for (auto& g : c.grid) v.push_back(grow(g)); return v; }())}, {"eps_perturbation", J::obj(pj)}})},
        {"stability", J::obj(sj)}, {"best_hierarchical", J::str(c.best_h)}, {"summary", joinv(sumj)}, {"external", J::obj(ext)},
        {"agreement", J::obj({{"ari_hier_vs_dbscan", J::num(c.agreement)}})},
        {"clusters", J::obj({{"hierarchical", cluster_describe_json(c, c.H[c.best_h].labels, true, 6)}, {"dbscan", cluster_describe_json(c, c.db_labels, true, 6)}})},
        {"noise", joinv(noisej)}, {"figures", J::strs(c.figures)}});

    write_file(out_dir + "/results.json", "{\"analysis\":" + c.analysis_json + ",\"docs\":" + c.docs_json + "}");
    write_file(out_dir + "/report.md", make_report(c));
    log("[6/6] wrote results.json and report.md");
    return cp;
}

}  // namespace hc
