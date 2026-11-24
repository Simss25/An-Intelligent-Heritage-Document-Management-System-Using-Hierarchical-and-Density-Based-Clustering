# Viva notes

**Why LSA before clustering?** TF-IDF has ~1000 sparse dimensions; distances there concentrate and DBSCAN becomes unreliable. TruncatedSVD gives a dense semantic space.

**Why Ward / average / complete / single?** Ward minimises variance (compact groups); single chains; complete limits diameter; average is in between. Comparing them shows linkage sensitivity.

**How was k chosen?** Silhouette over k = 2..12 for each linkage. If the best k is at the edge of the range, say so: there may be no clean answer.

**How were eps and MinPts chosen?** k-distance curves give the range; a grid is scored. Rule: 3-15 clusters, ≤30 % noise, then max silhouette. Silhouette on non-noise points flatters DBSCAN, so coverage is reported too.

**What is ARI stability?** Re-cluster random 80 % subsamples, compare to full-data labels with the Adjusted Rand Index.

**Why is PCA only for display?** Clustering uses all dimensions; PCA to 2D loses structure (see scree), so overlap in the plot is not proof of overlap in the data.

**Why is noise not deleted?** It is routed to a human reviewer: it contains junk scans but also minority-language and small collections.

**Biggest weakness?** Text excerpts are short, OCR is noisy, TF-IDF is English-centric, and a small sample may not represent the library.
