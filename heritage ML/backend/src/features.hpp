// features.hpp - preprocessing: missing values, text cleaning, TF-IDF, LSA, one-hot, scaling.
#pragma once
#include "data.hpp"

namespace hc {

using SparseRow = std::vector<std::pair<int, double>>;

struct MissRow { std::string field; int missing; double percent; };
struct Notes {
    int n_docs = 0, n_terms = 0, lsa_components = 0, n_missing_indicators = 0, cat_columns = 0, total_dims = 0;
    double lsa_share = 0;
    std::vector<MissRow> missing;
};
struct Features {
    Mat X;                                  // final feature matrix (n x d)
    std::vector<SparseRow> tfidf;           // unit-length TF-IDF rows
    std::vector<std::string> terms;
    std::unordered_map<std::string, int> vindex;
    Vec idf;
    std::vector<Doc> docs;
    std::vector<bool> desc_missing;
    Notes notes;
};

inline const std::set<std::string>& stopwords() {
    static const std::set<std::string> sw = {
        "a","about","above","after","again","against","all","also","am","an","and","any","are","as","at","be","because","been","before",
        "being","below","between","both","but","by","can","could","did","do","does","doing","down","during","each","few","for","from",
        "further","had","has","have","having","he","her","here","hers","herself","him","himself","his","how","i","if","in","into","is",
        "it","its","itself","me","more","most","my","myself","no","nor","not","of","off","on","once","only","or","other","our","ours",
        "ourselves","out","over","own","same","she","should","so","some","such","than","that","the","their","theirs","them","themselves",
        "then","there","these","they","this","those","through","to","too","under","until","up","very","was","we","were","what","when",
        "where","which","while","who","whom","why","will","with","would","you","your","yours","yourself","yourselves","may","might",
        "must","shall","upon","within","without","without","whose","however","therefore","thus","hence","among","amongst","via"};
    return sw;
}

// Lower-case, repair common OCR digit-for-letter swaps, drop digits/punctuation.
inline std::string clean_text(const std::string& in) {
    std::string s(in);
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    for (size_t i = 1; i + 1 < s.size(); ++i)
        if ((s[i] == '0' || s[i] == '1' || s[i] == '5') && s[i - 1] >= 'a' && s[i - 1] <= 'z' && s[i + 1] >= 'a' && s[i + 1] <= 'z')
            s[i] = s[i] == '1' ? 'l' : (s[i] == '0' ? 'o' : 's');
    std::string out; bool space = true;
    for (char c : s) {
        if (c >= 'a' && c <= 'z') { out += c; space = false; }
        else if (!space) { out += ' '; space = true; }
    }
    if (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

// unigrams + bigrams after stop-word removal (tokens of length >= 2)
inline std::vector<std::string> ngrams(const std::string& cleaned) {
    std::vector<std::string> toks, out;
    std::istringstream is(cleaned); std::string t;
    while (is >> t) if (t.size() >= 2 && !stopwords().count(t)) toks.push_back(t);
    for (size_t i = 0; i < toks.size(); ++i) {
        out.push_back(toks[i]);
        if (i + 1 < toks.size()) out.push_back(toks[i] + " " + toks[i + 1]);
    }
    return out;
}

inline Features build_features(const std::vector<Doc>& docs) {
    const Config& C = cfg();
    Features F; F.docs = docs;
    int n = (int)docs.size();
    F.notes.n_docs = n;

    // ---- missing-value report ----
    auto miss = [&](const std::string& f, int m) { F.notes.missing.push_back({f, m, std::round(m * 1000.0 / n) / 10.0}); };
    int m_desc = 0, m_lang = 0, m_year = 0, m_pages = 0, m_coll = 0;
    for (auto& d : docs) { m_desc += !d.has_desc; m_lang += !d.has_lang; m_year += isnan_(d.year); m_pages += isnan_(d.pages); m_coll += !d.has_coll; }
    miss("title", 0); miss("description", m_desc); miss("language", m_lang); miss("year", m_year); miss("pages", m_pages); miss("collection", m_coll);

    // ---- text: title counts double, description falls back to nothing ----
    std::vector<std::string> text(n);
    Vec text_len(n);
    for (int i = 0; i < n; ++i) {
        text[i] = clean_text(docs[i].title + " . " + docs[i].title + " . " + docs[i].desc);
        int words = 0; std::istringstream is(text[i]); std::string t; while (is >> t) ++words;
        text_len[i] = std::log1p((double)words);
        F.desc_missing.push_back(!docs[i].has_desc);
    }

    // ---- TF-IDF (sublinear tf, smooth idf, L2 norm) ----
    std::vector<std::unordered_map<std::string, int>> counts(n);
    std::unordered_map<std::string, int> df;
    for (int i = 0; i < n; ++i) {
        for (auto& g : ngrams(text[i])) counts[i][g]++;
        for (auto& kv : counts[i]) df[kv.first]++;
    }
    for (auto& kv : df)
        if (kv.second >= C.tfidf_min_df && kv.second <= C.tfidf_max_df * n) F.terms.push_back(kv.first);
    std::sort(F.terms.begin(), F.terms.end());
    int V = (int)F.terms.size();
    if (V < 3) throw std::runtime_error("vocabulary too small; need more text");
    F.idf.resize(V);
    for (int j = 0; j < V; ++j) { F.vindex[F.terms[j]] = j; F.idf[j] = std::log((1.0 + n) / (1.0 + df[F.terms[j]])) + 1.0; }
    F.tfidf.resize(n);
    for (int i = 0; i < n; ++i) {
        double norm = 0;
        for (auto& kv : counts[i]) {
            auto it = F.vindex.find(kv.first);
            if (it == F.vindex.end()) continue;
            double w = (1.0 + std::log((double)kv.second)) * F.idf[it->second];
            F.tfidf[i].push_back({it->second, w}); norm += w * w;
        }
        norm = std::sqrt(norm);
        std::sort(F.tfidf[i].begin(), F.tfidf[i].end());
        for (auto& e : F.tfidf[i]) e.second = norm > 0 ? e.second / norm : 0;
    }
    F.notes.n_terms = V;

    // ---- LSA: top-k subspace of the Gram matrix (subspace iteration + Ritz) ----
    int k = std::max(2, std::min({C.lsa_components, V - 1, n - 1}));
    int p = std::min(n, k + 8);
    auto Gmul = [&](const Vec& q) {
        Vec w(V, 0.0), r(n, 0.0);
        for (int i = 0; i < n; ++i) for (auto& e : F.tfidf[i]) w[e.first] += e.second * q[i];
        for (int i = 0; i < n; ++i) for (auto& e : F.tfidf[i]) r[i] += e.second * w[e.first];
        return r;
    };
    auto orthonormalise = [&](Mat& Q) {
        for (size_t a = 0; a < Q.size(); ++a) {
            for (size_t b = 0; b < a; ++b) { double d = 0; for (int i = 0; i < n; ++i) d += Q[a][i] * Q[b][i]; for (int i = 0; i < n; ++i) Q[a][i] -= d * Q[b][i]; }
            double nr = 0; for (int i = 0; i < n; ++i) nr += Q[a][i] * Q[a][i];
            nr = std::sqrt(nr); if (nr < 1e-12) nr = 1;
            for (int i = 0; i < n; ++i) Q[a][i] /= nr;
        }
    };
    std::mt19937 rng(C.seed); std::normal_distribution<double> nd(0, 1);
    Mat Q(p, Vec(n)); for (auto& col : Q) for (auto& x : col) x = nd(rng);
    orthonormalise(Q);
    for (int it = 0; it < 60; ++it) { for (auto& col : Q) col = Gmul(col); orthonormalise(Q); }
    Mat GQ(p); for (int a = 0; a < p; ++a) GQ[a] = Gmul(Q[a]);
    Mat T(p, Vec(p));
    for (int a = 0; a < p; ++a) for (int b = 0; b < p; ++b) { double d = 0; for (int i = 0; i < n; ++i) d += Q[a][i] * GQ[b][i]; T[a][b] = d; }
    for (int a = 0; a < p; ++a) for (int b = a + 1; b < p; ++b) T[a][b] = T[b][a] = 0.5 * (T[a][b] + T[b][a]);
    Vec lam; Mat W; jacobi_eigen(T, lam, W);
    Mat lsa(n, Vec(k, 0.0));
    double energy = 0;
    for (int j = 0; j < k; ++j) {
        double s = std::sqrt(std::max(lam[j], 0.0)); energy += std::max(lam[j], 0.0);
        for (int i = 0; i < n; ++i) { double u = 0; for (int a = 0; a < p; ++a) u += Q[a][i] * W[a][j]; lsa[i][j] = u * s; }
    }
    for (auto& row : lsa) { double nr = 0; for (double x : row) nr += x * x; nr = std::sqrt(nr); if (nr > 0) for (double& x : row) x /= nr; }
    F.notes.lsa_components = k; F.notes.lsa_share = energy / n;  // total energy of unit rows is n

    // ---- numeric block: median impute (+ missing flag), then standardise ----
    Mat num_cols;  // column-major
    Vec year(n), pages(n), tl = text_len;
    for (int i = 0; i < n; ++i) { year[i] = docs[i].year; pages[i] = isnan_(docs[i].pages) ? NaN : std::log1p(docs[i].pages); }
    for (Vec* col : {&year, &pages, &tl}) {
        bool any_missing = false; for (double x : *col) if (isnan_(x)) any_missing = true;
        double med = median(*col); Vec flag(n, 0.0), fill(*col);
        for (int i = 0; i < n; ++i) if (isnan_(fill[i])) { fill[i] = med; flag[i] = 1; }
        num_cols.push_back(fill);
        if (any_missing) { num_cols.push_back(flag); F.notes.n_missing_indicators++; }
    }
    for (auto& col : num_cols) {
        double mu = 0; for (double x : col) mu += x; mu /= n;
        double sd = 0; for (double x : col) sd += (x - mu) * (x - mu); sd = std::sqrt(sd / n); if (sd < 1e-12) sd = 1;
        for (double& x : col) x = (x - mu) / sd;
    }
    int nn = (int)num_cols.size();

    // ---- categorical block: one-hot with 'Unknown' ----
    std::vector<std::string> cats;  // "field=value"
    std::vector<std::vector<std::string>> vals(2);
    for (auto& d : docs) { vals[0].push_back(d.has_lang ? d.lang : "Unknown"); vals[1].push_back(d.has_coll ? d.collection : "Unknown"); }
    std::vector<std::pair<int, std::string>> levels;
    for (int f = 0; f < 2; ++f) { std::set<std::string> u(vals[f].begin(), vals[f].end()); for (auto& s : u) levels.push_back({f, s}); }
    int nc = (int)levels.size();
    F.notes.cat_columns = nc;

    F.X.assign(n, Vec());
    for (int i = 0; i < n; ++i) {
        Vec& r = F.X[i];
        for (double x : lsa[i]) r.push_back(C.w_text * x);
        for (int j = 0; j < nn; ++j) r.push_back(C.w_num * num_cols[j][i] / std::sqrt((double)nn));
        for (auto& lv : levels) r.push_back(C.w_cat * (vals[lv.first][i] == lv.second ? 1.0 : 0.0) / std::sqrt((double)std::max(nc, 1)));
    }
    F.notes.total_dims = (int)F.X[0].size();
    return F;
}

}  // namespace hc
