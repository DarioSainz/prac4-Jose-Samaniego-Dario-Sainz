#!/usr/bin/env python3
"""Genera las gráficas de la memoria a partir de results.csv.

Incluye una figura específica de Amdahl: curva teórica ajustada con la fracción
no paralelizable estimada y puntos medidos para OpenMP.
"""
import csv
import math
import statistics
from collections import defaultdict
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = Path(__file__).resolve().parent
CSV = HERE / "results.csv"
OUT = HERE.parent / "memoria" / "figures"
OUT.mkdir(parents=True, exist_ok=True)

groups = defaultdict(list)
with open(CSV) as f:
    for row in csv.DictReader(f):
        size = int(row["size"])
        threads = int(row["threads"])
        block_size = int(row.get("block_size") or 8)
        key = (row["variant"], size, threads, block_size)
        rec = {}
        for k, v in row.items():
            if k in ("variant", "size", "threads", "block_size", "rep"):
                continue
            if v == "":
                continue
            rec[k] = int(v)
        # Compatibilidad con results.csv antiguo.
        if "dct_local" not in rec and "dct_dir" in rec:
            rec["dct_local"] = rec["dct_dir"]
        if "dct_global" not in rec and "dct_dir" in rec:
            rec["dct_global"] = rec["dct_dir"]
        groups[key].append(rec)


def available_block_size():
    bs = sorted({k[3] for k in groups})
    return 8 if 8 in bs else (bs[0] if bs else 8)


BLOCK_SIZE = available_block_size()


def median_of(variant, size, threads, field, block_size=BLOCK_SIZE):
    rows = groups.get((variant, size, threads, block_size))
    if not rows:
        return None
    vals = [r[field] for r in rows if field in r]
    if not vals:
        return None
    return statistics.median(vals)


def has_variant(variant, block_size=BLOCK_SIZE):
    return any(k[0] == variant and k[3] == block_size for k in groups)


sizes = sorted({k[1] for k in groups if k[3] == BLOCK_SIZE})
thr_list = sorted({k[2] for k in groups if k[0] == "omp" and k[3] == BLOCK_SIZE})
if not sizes:
    raise SystemExit("No hay datos en results.csv")

# Fig 1 — Speed-up vs threads para OpenMP y, si está disponible, híbrido.
plt.figure(figsize=(8, 5))
markers = ["o", "s", "^", "D"]
for idx, size in enumerate(sizes):
    base = median_of("seq", size, 1, "total")
    if base is None:
        continue

    omp_x, omp_y = [], []
    for t in thr_list:
        val = median_of("omp", size, t, "total")
        if val:
            omp_x.append(t)
            omp_y.append(base / val)
    if omp_x:
        plt.plot(omp_x, omp_y, marker=markers[idx % len(markers)], linestyle="-",
                 label=f"OpenMP — {size}x{size}")

    if has_variant("hybrid"):
        hyb_x, hyb_y = [], []
        for t in thr_list:
            val = median_of("hybrid", size, t, "total")
            if val:
                hyb_x.append(t)
                hyb_y.append(base / val)
        if hyb_x:
            plt.plot(hyb_x, hyb_y, marker=markers[idx % len(markers)], linestyle="--",
                     label=f"Híbrido — {size}x{size}")

if thr_list:
    plt.plot(thr_list, thr_list, "k:", label="Speed-up ideal", alpha=0.5)
plt.xlabel("Número de hilos OpenMP")
plt.ylabel("Speed-up (T_seq / T_par)")
plt.title(f"Speed-up del pipeline completo vs nº de hilos (DCT block={BLOCK_SIZE})")
plt.grid(True, alpha=0.3)
plt.legend(fontsize=8, loc="upper left")
plt.tight_layout()
plt.savefig(OUT / "fig1_speedup_threads.png", dpi=140)
plt.close()

# Fig 2 — Speed-up por proceso para tamaño mayor disponible.
plt.figure(figsize=(9, 5))
size = 1024 if 1024 in sizes else max(sizes)
processes = [
    ("SRM 3x3", "srm3"),
    ("SRM 5x5", "srm5"),
    ("ELA", "ela"),
    ("DCT inv", "dct_inv"),
    ("DCT local", "dct_local"),
    ("DCT global", "dct_global"),
    ("TOTAL", "total"),
]
labels = [p[0] for p in processes]
seq_times = [median_of("seq", size, 1, p[1]) for p in processes]
series = []
representative_t = 2 if 2 in thr_list else (thr_list[0] if thr_list else 1)
for variant, label, threads in [
    ("omp", f"OpenMP T={representative_t}", representative_t),
    ("async_only", "std::async", 1),
    ("hybrid", f"Híbrido T={representative_t}", representative_t),
]:
    if not has_variant(variant):
        continue
    vals = [median_of(variant, size, threads, p[1]) for p in processes]
    if all(v is None for v in vals):
        continue
    speedups = [s / v if (s and v) else 0 for s, v in zip(seq_times, vals)]
    series.append((label, speedups))

x = list(range(len(labels)))
width = 0.8 / max(1, len(series))
for idx, (label, vals) in enumerate(series):
    offset = (idx - (len(series)-1)/2) * width
    plt.bar([i + offset for i in x], vals, width=width, label=label)

plt.xticks(x, labels, rotation=15)
plt.ylabel("Speed-up vs secuencial")
plt.title(f"Speed-up por proceso (imagen {size}x{size}, block={BLOCK_SIZE})")
plt.axhline(y=1.0, color="k", linestyle=":", alpha=0.5, label="Sin mejora")
plt.grid(True, alpha=0.3, axis="y")
plt.legend()
plt.tight_layout()
plt.savefig(OUT / "fig2_speedup_por_proceso.png", dpi=140)
plt.close()

# Fig 3 — Eficiencia OpenMP.
plt.figure(figsize=(8, 5))
for size in sizes:
    base = median_of("seq", size, 1, "total")
    if base is None:
        continue
    xs, ys = [], []
    for t in thr_list:
        val = median_of("omp", size, t, "total")
        if val:
            xs.append(t)
            ys.append((base / val) / t)
    if xs:
        plt.plot(xs, ys, marker="o", label=f"{size}x{size}")
plt.axhline(y=1.0, color="k", linestyle=":", alpha=0.5, label="Eficiencia ideal")
plt.xlabel("Número de hilos")
plt.ylabel("Eficiencia E = S/p")
plt.title("Eficiencia de la paralelización OpenMP")
plt.grid(True, alpha=0.3)
plt.legend()
plt.tight_layout()
plt.savefig(OUT / "fig3_eficiencia.png", dpi=140)
plt.close()

# Fig 4 — Reparto secuencial por proceso.
plt.figure(figsize=(8, 4.8))
size = 1024 if 1024 in sizes else max(sizes)
labels = ["SRM 3x3", "SRM 5x5", "ELA", "DCT inv", "DCT local", "DCT global"]
fields = ["srm3", "srm5", "ela", "dct_inv", "dct_local", "dct_global"]
seq_vals = [median_of("seq", size, 1, f) for f in fields]
seq_vals = [0 if v is None else v for v in seq_vals]
plt.bar(labels, seq_vals)
total = sum(seq_vals) or 1
ymax = max(seq_vals) if seq_vals else 1
plt.ylim(0, ymax * 1.28)
for i, v in enumerate(seq_vals):
    plt.text(i, v + ymax * 0.035, f"{v}ms\n({100*v/total:.1f}%)", ha="center", fontsize=8)
plt.xticks(rotation=15)
plt.ylabel("Tiempo mediano (ms)")
plt.title(f"Reparto de tiempo secuencial por etapa ({size}x{size})")
plt.grid(True, alpha=0.3, axis="y")
plt.tight_layout()
plt.savefig(OUT / "fig4_reparto_tiempos.png", dpi=140)
plt.close()

# Fig 5 — Tiempo total vs talla.
plt.figure(figsize=(8, 5))
for variant, label, threads, ls in [
    ("seq", "Secuencial", 1, "-"),
    ("async_only", "std::async", 1, "--"),
    ("omp", f"OpenMP T={representative_t}", representative_t, "-"),
    ("hybrid", f"Híbrido T={representative_t}", representative_t, ":"),
]:
    if not has_variant(variant):
        continue
    xs, ys = [], []
    for s in sizes:
        v = median_of(variant, s, threads, "total")
        if v is not None:
            xs.append(s)
            ys.append(v)
    if xs:
        plt.plot(xs, ys, marker="o", linestyle=ls, label=label)
plt.xlabel("Lado de la imagen (px)")
plt.ylabel("Tiempo total (ms)")
plt.title("Tiempo total del pipeline vs talla del problema")
plt.grid(True, alpha=0.3)
plt.legend()
plt.tight_layout()
plt.savefig(OUT / "fig5_tiempos_vs_talla.png", dpi=140)
plt.close()

# Fig 6 — Amdahl teórico vs medido.
size = 1024 if 1024 in sizes else max(sizes)
base = median_of("seq", size, 1, "total")
points = []
if base:
    for p in thr_list:
        val = median_of("omp", size, p, "total")
        if val and p >= 1:
            points.append((p, base / val))

if len(points) >= 2:
    f_estimates = []
    for p, speedup in points:
        if p <= 1 or speedup <= 0:
            continue
        # De S = 1/(f + (1-f)/p) se despeja f.
        f = (1.0 / speedup - 1.0 / p) / (1.0 - 1.0 / p)
        if math.isfinite(f):
            f_estimates.append(max(0.0, min(1.0, f)))
    if f_estimates:
        f = statistics.median(f_estimates)
        max_p = max(max(thr_list), 8)
        xs = list(range(1, max_p + 1))
        ys = [1.0 / (f + (1.0 - f) / p) for p in xs]
        plt.figure(figsize=(8, 5))
        plt.plot(xs, ys, linestyle="-", label=f"Amdahl teórico f={f:.3f}; Smax≈{1/f if f > 0 else float('inf'):.2f}x")
        plt.scatter([p for p, _ in points], [s for _, s in points], label="Puntos medidos OpenMP")
        plt.plot(xs, xs, "k:", alpha=0.5, label="Speed-up ideal")
        plt.xlabel("Unidades de cómputo / hilos p")
        plt.ylabel("Speed-up S(p)")
        plt.title(f"Amdahl teórico vs medido ({size}x{size}, block={BLOCK_SIZE})")
        plt.grid(True, alpha=0.3)
        plt.legend()
        plt.tight_layout()
        plt.savefig(OUT / "fig6_amdahl_teorico_vs_medido.png", dpi=140)
        plt.close()

print("Gráficas guardadas en:", OUT)
for f in sorted(OUT.glob("*.png")):
    print("  ", f.name)
