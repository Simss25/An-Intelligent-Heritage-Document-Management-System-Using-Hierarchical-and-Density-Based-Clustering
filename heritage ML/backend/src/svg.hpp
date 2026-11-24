// svg.hpp - tiny SVG drawing layer. All figures are plain .svg files (no graphics library needed).
#pragma once
#include "util.hpp"

namespace hc {

inline const char* INK = "#211d18";
inline const char* PAPER = "#f6f0e4";
inline const char* RULE = "#cbbfa8";
inline const char* NOISE_C = "#8d8d8d";
inline const std::vector<std::string>& palette() {
    static const std::vector<std::string> p = {"#a4452c", "#2f5d50", "#c08a2b", "#2b3a67", "#6b3a5b", "#7a7a2e", "#3d7f9a", "#8c5a3c",
                                               "#4f6f3a", "#b5707a", "#5c4b99", "#9a6b00", "#35605f", "#8a2f4d", "#667085"};
    return p;
}
inline std::string colour(int label) { return label < 0 ? NOISE_C : palette()[label % palette().size()]; }

inline std::string xml(const std::string& s) {
    std::string o; for (char c : s) { if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;"; else if (c == '"') o += "&quot;"; else o += c; }
    return o;
}
inline std::string f1(double v) { char b[32]; std::snprintf(b, sizeof b, "%.1f", v); return b; }
inline std::string fx(double v, int d) { char b[32]; std::snprintf(b, sizeof b, "%.*f", d, v); return b; }

struct Svg {
    int W, H; std::ostringstream o;
    Svg(int w, int h) : W(w), H(h) {
        o << "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 " << w << " " << h << "' width='" << w << "' height='" << h
          << "' font-family=\"Georgia,'Times New Roman',serif\"><rect width='100%' height='100%' fill='" << PAPER << "'/>";
    }
    void line(double x1, double y1, double x2, double y2, const std::string& st = INK, double sw = 1, const std::string& dash = "") {
        o << "<line x1='" << f1(x1) << "' y1='" << f1(y1) << "' x2='" << f1(x2) << "' y2='" << f1(y2) << "' stroke='" << st << "' stroke-width='" << sw << "'";
        if (!dash.empty()) o << " stroke-dasharray='" << dash << "'";
        o << "/>";
    }
    void rect(double x, double y, double w, double h, const std::string& fill, const std::string& stroke = "none", double sw = 1, double op = 1) {
        o << "<rect x='" << f1(x) << "' y='" << f1(y) << "' width='" << f1(std::max(w, 0.0)) << "' height='" << f1(std::max(h, 0.0)) << "' fill='" << fill
          << "' stroke='" << stroke << "' stroke-width='" << sw << "' fill-opacity='" << op << "'/>";
    }
    void circle(double cx, double cy, double r, const std::string& fill, double op = 0.9) {
        o << "<circle cx='" << f1(cx) << "' cy='" << f1(cy) << "' r='" << r << "' fill='" << fill << "' fill-opacity='" << op << "' stroke='" << INK << "' stroke-width='0.3'/>";
    }
    void cross(double cx, double cy, double r, const std::string& st) {
        line(cx - r, cy - r, cx + r, cy + r, st, 1.3); line(cx - r, cy + r, cx + r, cy - r, st, 1.3);
    }
    void text(double x, double y, const std::string& s, double size = 11, const std::string& anchor = "start", const std::string& weight = "normal",
              const std::string& fill = INK, double rot = 0) {
        o << "<text x='" << f1(x) << "' y='" << f1(y) << "' font-size='" << size << "' text-anchor='" << anchor << "' font-weight='" << weight << "' fill='" << fill << "'";
        if (rot != 0) o << " transform='rotate(" << rot << " " << f1(x) << " " << f1(y) << ")'";
        o << ">" << xml(s) << "</text>";
    }
    void polyline(const std::vector<std::pair<double, double>>& pts, const std::string& st, double sw = 1.5) {
        o << "<polyline fill='none' stroke='" << st << "' stroke-width='" << sw << "' points='";
        for (auto& p : pts) o << f1(p.first) << "," << f1(p.second) << " ";
        o << "'/>";
    }
    void raw(const std::string& s) { o << s; }
    std::string str() { return o.str() + "</svg>"; }
};

// "Nice" tick positions covering [lo, hi].
inline Vec nice_ticks(double lo, double hi, int target = 5) {
    if (hi <= lo) hi = lo + 1;
    double raw = (hi - lo) / target, mag = std::pow(10, std::floor(std::log10(raw))), f = raw / mag;
    double step = (f < 1.5 ? 1 : f < 3 ? 2 : f < 7 ? 5 : 10) * mag;
    Vec t; for (double v = std::ceil(lo / step) * step; v <= hi + 1e-9; v += step) t.push_back(std::fabs(v) < 1e-12 ? 0 : v);
    return t;
}

// A plotting rectangle with a linear data->pixel mapping.
struct Axes {
    double x, y, w, h, x0, x1, y0, y1;
    double px(double v) const { return x + (v - x0) / (x1 - x0 == 0 ? 1 : x1 - x0) * w; }
    double py(double v) const { return y + h - (v - y0) / (y1 - y0 == 0 ? 1 : y1 - y0) * h; }
    void frame(Svg& s, const std::string& xl = "", const std::string& yl = "", bool grid = true, int dx = 2, int dy = 2) const {
        for (double t : nice_ticks(y0, y1, 5)) {
            if (grid) s.line(x, py(t), x + w, py(t), RULE, 0.5);
            s.text(x - 6, py(t) + 4, fx(t, dy), 10, "end");
        }
        for (double t : nice_ticks(x0, x1, 6)) s.text(px(t), y + h + 15, fx(t, dx), 10, "middle");
        s.line(x, y, x, y + h); s.line(x, y + h, x + w, y + h);
        if (!xl.empty()) s.text(x + w / 2, y + h + 32, xl, 11, "middle");
        if (!yl.empty()) s.text(x - 40, y + h / 2, yl, 11, "middle", "normal", INK, -90);
    }
};

inline std::string heat_colour(double t) {  // 0 -> paper, 1 -> deep rust
    t = std::min(1.0, std::max(0.0, t));
    int r = (int)(246 + (130 - 246) * t), g = (int)(240 + (34 - 240) * t), b = (int)(228 + (20 - 228) * t);
    char buf[16]; std::snprintf(buf, sizeof buf, "#%02x%02x%02x", r, g, b); return buf;
}

}  // namespace hc
