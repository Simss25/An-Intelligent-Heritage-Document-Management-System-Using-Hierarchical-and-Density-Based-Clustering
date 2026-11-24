# Heritage Document Topic Grouping: Hierarchical and Density-Based Clustering (C++ implementation)

Corpus file: `uploaded.csv` (48 documents). Random seed: 42. Figures are in `outputs/figures/`; run `heritage analyze` to regenerate this file.

## 1. Problem

Digital libraries such as the National Digital Library of India hold millions of scanned books, gazetteers, manuscripts and pamphlets whose catalogue records are incomplete, inconsistently labelled and in many languages. This project groups scans into topical shelves **without using subject labels**, and flags documents that fit no group so a human cataloguer can review them.

## 2. Data preparation

| Field | Missing | Handling |
|---|---|---|
| title | 0 (0.0%) | required, never imputed |
| description | 0 (0.0%) | title text only; flag kept |
| language | 0 (0.0%) | category `Unknown`, then one-hot |
| year | 0 (0.0%) | median impute + missing flag + standardise |
| pages | 0 (0.0%) | median impute, log, standardise |
| collection | 0 (0.0%) | category `Unknown`, then one-hot |

Text is cleaned (lower-case, OCR digit/letter swaps repaired, punctuation removed), converted to TF-IDF (319 terms, 1-2 grams, sublinear tf, smooth idf) and compressed with LSA to 15 dimensions (89% of TF-IDF signal retained). Year, log-pages and log-length are standardised; language and collection are one-hot encoded. Block weights: text=1, numeric=0.2, categorical=0.2. Final matrix: 24 dimensions.

## 3. Hierarchical clustering

| Linkage | Best k | Silhouette | Cophenetic corr. | Smallest / largest | Stability (ARI) |
|---|---|---|---|---|---|
| ward | 12 | 0.69 | 0.97 | 2 / 6 | 0.92 ± 0.05 |
| complete | 12 | 0.69 | 0.99 | 2 / 6 | 0.96 ± 0.04 |
| average | 12 | 0.69 | 0.99 | 2 / 6 | 0.98 ± 0.03 |
| single | 12 | 0.69 | 0.99 | 2 / 6 | 0.98 ± 0.03 |

Highest silhouette: **ward**. Single linkage tends to chain (tiny clusters plus one giant one); ward and average give more balanced groups. The result is **linkage-sensitive** and should not be reported as one 'true' structure.

## 4. DBSCAN

Grid: MinPts in {3, 4, 5, 8}, 18 epsilon values per MinPts from the k-distance curves. Selection rule: 3-15 clusters, noise <= 30%, then highest silhouette (noise excluded).

Selected: eps = 0.618, MinPts = 3 -> 6 clusters, 29% noise, silhouette 0.72.

| eps x | Clusters | Noise | ARI vs selected |
|---|---|---|---|
| 0.8 | 6 | 38% | 0.76 |
| 0.9 | 6 | 33% | 0.88 |
| 1.1 | 6 | 29% | 1.00 |
| 1.25 | 6 | 25% | 0.89 |

Small changes of epsilon move many documents between 'cluster' and 'noise'. Silhouette on non-noise points rewards settings that discard awkward documents, so coverage (1 - noise) must be read with it.

## 5. Comparison

| Method | Clusters | Noise | Silhouette | Stability |
|---|---|---|---|---|
| hier-ward | 12 | 0% | 0.69 | 0.92 |
| hier-complete | 12 | 0% | 0.69 | 0.96 |
| hier-average | 12 | 0% | 0.69 | 0.98 |
| hier-single | 12 | 0% | 0.69 | 0.98 |
| dbscan | 6 | 29% | 0.72 | 0.96 |

ARI between best hierarchical labels and DBSCAN on documents DBSCAN placed: 1.00. This ignores the noise documents, and DBSCAN's dense cores are subsets of larger hierarchical groups.

External sanity check against catalogue subjects (not used for clustering; the labels are themselves imperfect):

| Method | ARI | NMI |
|---|---|---|
| hier-ward | 0.47 | 0.74 |
| hier-complete | 0.47 | 0.74 |
| hier-average | 0.47 | 0.74 |
| hier-single | 0.47 | 0.74 |
| dbscan | 0.67 | 0.86 |

## 6. Noise observations

DBSCAN labelled 14 documents as noise. Examples:

- **Classical Sanskrit Drama and Poetry**: The catalogue record covers Sanskrit grammar, ancient manuscripts, classical poetry, epic 
- **Classical Sanskrit Drama and Poetry (Volume 2)**: The catalogue record covers Sanskrit grammar, ancient manuscripts, classical poetry, epic 
- **Agricultural Census and Crop Survey**: The document records village land, farming, crop production, irrigation, agricultural taxa
- **Village Land Holdings Register**: The document records village land, farming, crop production, irrigation, agricultural taxa
- **Colonial Agriculture Statistics**: The document records village land, farming, crop production, irrigation, agricultural taxa
- **Agricultural Census and Crop Survey (Volume 2)**: The document records village land, farming, crop production, irrigation, agricultural taxa
- **Village Land Holdings Register (Volume 2)**: The document records village land, farming, crop production, irrigation, agricultural taxa
- **Colonial Agriculture Statistics (Volume 2)**: The document records village land, farming, crop production, irrigation, agricultural taxa

Typical noise: blank leaves, ledger or index fragments, advertisements, badly damaged OCR and mixed-subject volumes. Noise is treated as a **review queue**, not an error.

## 7. Limitations

- **Sample**: a small sample is not the library; cluster counts and shapes change with more documents.
- **Excerpts, not books**: titles and short OCR excerpts carry thin signal and are dominated by catalogue wording.
- **OCR quality**: damaged text clusters with other damaged text, not by topic.
- **Language**: TF-IDF with English stop-words serves other languages poorly; non-English records may group by transliteration habit.
- **Silhouette is not truth**: it favours compact convex groups and cannot judge historical meaning.
- **Parameters**: linkage, k, epsilon, MinPts and feature weights all change the result.
- **Distance concentration** in high dimensions hurts density methods; LSA reduces but does not remove it.

## 8. Bias and ethical considerations

- **Whose heritage is visible?** Digitisation and cataloguing priorities favour some languages, regions and communities; clusters reproduce that skew.
- **Colonial vocabulary**: administrative reports describe people in the administrators' terms; surfacing groups uncritically amplifies that framing.
- **Minority-language invisibility**: small collections look like noise to density methods, so noise must go to human review, never to deletion.
- **Metadata features**: language and year can pull documents together for non-topical reasons; their weights are documented and low.
- **Automation boundary**: output is a suggestion for librarians; no document should be hidden or ranked lower because of its label.
- **Copyright and access**: use only openly licensed scans and keep links to the original records.

## 9. Conclusions

Hierarchical clustering gives a complete grouping with nested structure but depends on linkage. DBSCAN found 6 dense groups and isolated 29% of documents as noise, useful for quality control but very sensitive to epsilon. Using both gives stable shelves where methods agree and a review queue where they do not.
