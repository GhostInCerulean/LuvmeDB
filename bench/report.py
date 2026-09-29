#!/usr/bin/env python3
import json, sys, re, collections

src = sys.argv[1] if len(sys.argv) > 1 else "results.json"
out = sys.argv[2] if len(sys.argv) > 2 else "report.html"
data = json.load(open(src))

# series name -> [(arg, time_ns)]
series = collections.defaultdict(list)
for b in data["benchmarks"]:
    if b.get("aggregate_name") not in (None, "median"):
        continue  # with aggregates: keep only the median
    name = b["run_name"]
    m = re.match(r"(.+?)/(\d+)", name)
    base, arg = (m.group(1), int(m.group(2))) if m else (name, 0)
    unit = {"ns": 1, "us": 1e3, "ms": 1e6, "s": 1e9}[b["time_unit"]]
    series[base].append((arg, b["real_time"] * unit))

traces = [
    {"x": [a for a, _ in sorted(pts)], "y": [t for _, t in sorted(pts)],
     "name": name, "mode": "lines+markers", "type": "scatter"}
    for name, pts in series.items()
]
layout = {"title": "LuvmeDB benchmarks (median)",
          "xaxis": {"title": "input size", "type": "log"},
          "yaxis": {"title": "time (ns)", "type": "log"}}

html = f"""<!doctype html><meta charset="utf-8">
<script src="https://cdn.plot.ly/plotly-2.35.2.min.js"></script>
<div id="c" style="width:100%;height:90vh"></div>
<script>Plotly.newPlot("c", {json.dumps(traces)}, {json.dumps(layout)});</script>"""
open(out, "w").write(html)
print("wrote", out)