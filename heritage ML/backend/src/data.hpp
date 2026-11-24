// data.hpp - corpus loading. Accepts NDLI-style column names; only `title` is required.
#pragma once
#include "util.hpp"

namespace hc {

struct Doc {
    std::string id, title, desc, lang, collection, subject;
    bool has_desc = false, has_lang = false, has_coll = false, has_subject = false;
    double year = NaN, pages = NaN;
};

inline double first_year(const std::string& s) {
    for (size_t i = 0; i + 3 < s.size(); ++i) {
        if (!std::isdigit((unsigned char)s[i])) continue;
        bool ok = std::isdigit((unsigned char)s[i + 1]) && std::isdigit((unsigned char)s[i + 2]) && std::isdigit((unsigned char)s[i + 3]);
        if (!ok) continue;
        int y = std::stoi(s.substr(i, 4));
        if ((y >= 1000 && y <= 1999) || (y >= 2000 && y <= 2029)) return y;
    }
    return NaN;
}
inline double first_number(const std::string& s) {
    for (size_t i = 0; i < s.size(); ++i)
        if (std::isdigit((unsigned char)s[i])) { size_t j = i; while (j < s.size() && std::isdigit((unsigned char)s[j])) ++j; return std::stod(s.substr(i, j - i)); }
    return NaN;
}

inline std::vector<Doc> load_corpus(const std::string& path) {
    auto rows = parse_csv(read_file(path));
    if (rows.size() < 2) throw std::runtime_error("CSV has no data rows");
    std::map<std::string, int> col;
    for (size_t i = 0; i < rows[0].size(); ++i) col[lower(trim(rows[0][i]))] = (int)i;
    auto find = [&](std::initializer_list<const char*> names) {
        for (auto n : names) { auto it = col.find(n); if (it != col.end()) return it->second; }
        return -1;
    };
    int c_title = find({"title", "name", "document title"});
    int c_desc = find({"description", "abstract", "ocr_text", "text", "summary", "excerpt"});
    int c_lang = find({"language", "lang"});
    int c_year = find({"year", "date", "publication year", "publication date", "date issued"});
    int c_pages = find({"pages", "page count", "extent", "no of pages"});
    int c_coll = find({"collection", "source", "type", "resource type"});
    int c_subj = find({"reference_topic", "subject", "subject classification", "topic"});
    int c_id = find({"doc_id", "id", "identifier", "handle"});
    if (c_title < 0) throw std::runtime_error("CSV needs a 'title' column");

    auto cell = [&](const std::vector<std::string>& r, int c, bool& has) {
        has = false;
        if (c < 0 || c >= (int)r.size() || is_missing_token(r[c])) return std::string();
        has = true; return trim(r[c]);
    };
    std::vector<Doc> docs;
    std::set<std::pair<std::string, std::string>> seen;
    for (size_t i = 1; i < rows.size(); ++i) {
        const auto& r = rows[i];
        Doc d; bool h;
        d.title = cell(r, c_title, h);
        if (!h) continue;
        d.desc = cell(r, c_desc, d.has_desc);
        d.lang = cell(r, c_lang, d.has_lang);
        d.collection = cell(r, c_coll, d.has_coll);
        d.subject = cell(r, c_subj, d.has_subject);
        std::string y = cell(r, c_year, h); if (h) d.year = first_year(y);
        std::string p = cell(r, c_pages, h); if (h) d.pages = first_number(p);
        std::string id = cell(r, c_id, h);
        char buf[16]; std::snprintf(buf, sizeof buf, "DOC%04d", (int)docs.size());
        d.id = h ? id : buf;
        if (!seen.insert({d.title, d.desc}).second) continue;  // drop duplicates
        docs.push_back(d);
    }
    if (docs.size() < 12) throw std::runtime_error("need at least 12 documents");
    return docs;
}

}  // namespace hc
