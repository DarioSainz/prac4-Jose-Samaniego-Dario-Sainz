#!/usr/bin/env python3
"""Genera diagramas (dependencias y flujo paralelo) usando matplotlib."""
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Rectangle

OUT = Path(__file__).resolve().parent.parent / "memoria" / "figures"
OUT.mkdir(parents=True, exist_ok=True)

def box(ax, x, y, w, h, text, color="#4c78a8", fontsize=10, fontcolor="white"):
    p = FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02", linewidth=1.2,
                       facecolor=color, edgecolor="black")
    ax.add_patch(p)
    ax.text(x + w/2, y + h/2, text, ha="center", va="center",
            fontsize=fontsize, color=fontcolor, weight='bold')

def arrow(ax, x1, y1, x2, y2, color="black", style="->", lw=1.4):
    ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2), arrowstyle=style, lw=lw,
                                 color=color, mutation_scale=14))

# =========================================================================
# Diagrama 1 — Dependencias del pipeline secuencial
# =========================================================================
fig, ax = plt.subplots(figsize=(11, 5))
ax.set_xlim(0, 11); ax.set_ylim(0, 5); ax.axis('off')

box(ax, 0.2, 2.0, 1.6, 1.0, "load_from_file\n(imagen)", color="#666666")
# 6 procesos paralelos
processes = [
    ("compute_srm\n(3x3)",   2.4, 4.2, "#4c78a8"),
    ("compute_srm\n(5x5)",   2.4, 3.0, "#4c78a8"),
    ("compute_ela\n(q=90)",  2.4, 1.8, "#54a24b"),
    ("compute_dct\n(inversa)", 5.0, 4.2, "#e45756"),
    ("compute_dct\n(local)",   5.0, 3.0, "#e45756"),
    ("compute_dct\n(global)",  5.0, 1.8, "#e45756"),
]
for txt, x, y, c in processes:
    box(ax, x, y, 1.6, 0.8, txt, color=c)

# Final saves
saves = [
    ("save srm_3x3.png", 7.5, 4.35),
    ("save srm_5x5.png", 7.5, 3.65),
    ("save ela.png",     7.5, 2.95),
    ("save dct_inv.png", 7.5, 2.25),
    ("save dct_local.png", 7.5, 1.55),
    ("save dct_global.png", 7.5, 0.85),
]
for txt, x, y in saves:
    box(ax, x, y, 1.7, 0.5, txt, color="#888888", fontsize=9)

# arrows from load to processes
for _, x, y, _ in processes:
    arrow(ax, 1.8, 2.5, x, y + 0.4)

# arrows from processes to saves (matching order)
arrow(ax, 4.0, 4.6, 7.5, 4.6)
arrow(ax, 4.0, 3.4, 7.5, 3.9)
arrow(ax, 4.0, 2.2, 7.5, 3.2)
arrow(ax, 6.6, 4.6, 7.5, 2.5)
arrow(ax, 6.6, 3.4, 7.5, 1.8)
arrow(ax, 6.6, 2.2, 7.5, 1.1)

ax.text(5.5, 4.85, "Procesos independientes (mismo input → outputs distintos)",
        ha="center", fontsize=11, style="italic")
ax.set_title("Diagrama de dependencias — pipeline secuencial", fontsize=13, weight='bold')
plt.tight_layout()
plt.savefig(OUT / "fig_dependencias.png", dpi=140, bbox_inches="tight")
plt.close()

# =========================================================================
# Diagrama 2 — Grafo de control de flujo paralelo (OpenMP + std::async)
# =========================================================================
fig, ax = plt.subplots(figsize=(12, 7))
ax.set_xlim(0, 12); ax.set_ylim(0, 7); ax.axis('off')

# Carga (single thread)
box(ax, 0.3, 3.0, 1.7, 0.9, "load_from_file\n(thread principal)", color="#666666")

# Punto de fork std::async
box(ax, 2.5, 3.0, 1.6, 0.9, "FORK\n(std::async ×6)", color="#222222")
arrow(ax, 2.0, 3.45, 2.5, 3.45)

# 6 procesos en banda paralela
proc_y = [6.0, 5.0, 4.0, 3.0, 2.0, 1.0]
proc_labels = [
    "Tarea 1: SRM 3x3\n(convolución + abs + norm)",
    "Tarea 2: SRM 5x5\n(convolución + abs + norm)",
    "Tarea 3: ELA q=90\n(comprime + resta + abs)",
    "Tarea 4: DCT inversa\n(bucle bloques)",
    "Tarea 5: DCT local\n(norm. por bloque)",
    "Tarea 6: DCT global\n(norm. global)",
]
proc_colors = ["#4c78a8", "#4c78a8", "#54a24b", "#e45756", "#e45756", "#e45756"]
for y, label, c in zip(proc_y, proc_labels, proc_colors):
    box(ax, 4.5, y - 0.35, 3.7, 0.9, label, color=c, fontsize=9)
    arrow(ax, 4.1, 3.45, 4.5, y, lw=1.0, color="#444444")

# Anotación: dentro de cada banda, OpenMP paraleliza
for y in proc_y:
    ax.text(8.3, y, "← bucles internos paralelos\n   (OpenMP: píxeles/bloques)",
            fontsize=8.5, va='center', style='italic', color='#444444')

# JOIN
box(ax, 9.6, 3.0, 1.6, 0.9, "JOIN\n(future.get())", color="#222222")
for y in proc_y:
    arrow(ax, 8.2, y, 9.6, 3.45, lw=1.0, color="#444444")

# Save final
box(ax, 11.0, 3.0, 0.8, 0.9, "save_*", color="#666666", fontsize=9)
arrow(ax, 11.2, 3.45, 11.0, 3.45)

# Título y leyenda
ax.text(6.0, 6.7, "Esquema fork–join — paralelismo funcional (std::async) anidado con paralelismo de datos (OpenMP)",
        ha="center", fontsize=11, weight='bold')
ax.text(6.0, 0.4, "Cada banda horizontal corresponde a un hilo lanzado por std::async; "
                  "dentro de cada uno OpenMP reparte los bucles entre los hilos del pool.",
        ha="center", fontsize=9, style="italic", color="#444444")

plt.tight_layout()
plt.savefig(OUT / "fig_paralelo.png", dpi=140, bbox_inches="tight")
plt.close()

# =========================================================================
# Diagrama 3 — Diagrama de flujo de un proceso (compute_srm)
# =========================================================================
fig, ax = plt.subplots(figsize=(9, 7))
ax.set_xlim(0,9); ax.set_ylim(0,8); ax.axis('off')

steps = [
    ("Entrada: Image<unsigned char>", "#999999"),
    ("to_grayscale()  — promedio ponderado RGB", "#4c78a8"),
    ("convert<float>()  — cambio de tipo", "#4c78a8"),
    ("convolution(kernel SRM)  ★ bucle más caro", "#e45756"),
    ("abs()  — |valor|", "#4c78a8"),
    ("normalized()  — reducción min/max + escala", "#54a24b"),
    ("operator*255  — escala a 0-255", "#4c78a8"),
    ("convert<unsigned char>()", "#4c78a8"),
    ("Salida: Image<unsigned char>", "#999999"),
]
y = 7
for label, color in steps:
    is_critical = "★" in label
    box(ax, 1.2, y, 6.6, 0.65, label, color=color, fontsize=10)
    if is_critical:
        ax.text(8.0, y + 0.32, "← O(W·H·K²)", fontsize=9, color="#a04040", weight='bold')
    if y > 0.5:
        arrow(ax, 4.5, y, 4.5, y - 0.15, lw=1.2)
    y -= 0.85

ax.set_title("Diagrama de flujo — compute_srm()", fontsize=13, weight='bold')
ax.text(4.5, -0.2, "Las cajas en azul son píxel-a-píxel; en verde la reducción min/max; la DCT se reparte por bloques.",
        ha="center", fontsize=9, style="italic", color="#444444")
plt.tight_layout()
plt.savefig(OUT / "fig_flujo_srm.png", dpi=140, bbox_inches="tight")
plt.close()

print("Diagramas guardados:")
for f in sorted(OUT.glob("fig_*.png")):
    print("  ", f.name)
