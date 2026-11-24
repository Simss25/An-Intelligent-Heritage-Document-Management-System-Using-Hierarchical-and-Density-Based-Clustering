// context.hpp - everything one analysis run produces (shared by figures, report, API).
#pragma once
#include "metrics.hpp"
#include "svg.hpp"

namespace hc {

struct HierResult {
    Tree tree; Vec sil_by_k; int best_k = 0; double sil = NaN; std::vector<int> labels; std::vector<int> sizes;
};
struct GridRow { double eps; int min_pts; int n_clusters; double noise; double sil; };
struct SummaryRow { std::string method; int clusters; double noise, sil, stab; int smallest, largest; double coph; };
struct PerturbRow { double factor; double ari; int n_clusters; double noise; };
struct ExtRow { std::string method; double ari, nmi, coverage; };

struct Context {
    std::string csv_name;
    std::vector<Doc> docs;
    Features F;
    DistMat D;
    Mat xy;                       // 2-D PCA projection (for display only)
    Vec pca_ratio;                // explained-variance ratio of every PCA component
    std::map<std::string, HierResult> H;
    Vec eps_values; std::vector<GridRow> grid; GridRow best_db{};
    std::vector<int> db_labels;
    std::map<std::string, Stab> stab;
    std::vector<PerturbRow> perturb;
    std::string best_h;
    std::vector<SummaryRow> summary;
    std::vector<ExtRow> external;
    double agreement = NaN;
    bool has_ref = false;
    std::vector<std::string> ref;  // catalogue subjects (validation only)
    std::vector<int> noise_idx;
    std::vector<std::string> figures;
    std::string analysis_json, docs_json;
};

}  // namespace hc
