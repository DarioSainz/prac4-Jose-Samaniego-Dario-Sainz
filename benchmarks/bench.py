#!/usr/bin/env python3
"""Ejecuta benchmarks reproducibles del pipeline.

Por defecto mide el coste computacional de las etapas principales y omite la
escritura final de imágenes con --no-output. Así las medidas reflejan mejor el
paralelismo de OpenMP/std::async y no la variabilidad de la E/S de disco.

Uso recomendado en laboratorio/portátil:
    python3 bench.py --reps 10 --warmup 2 --sizes 256 512 1024 --threads 1 2 4 8 \
        --taskset-cpus 0-7 --nice-level -10
"""
import argparse
import csv
import pathlib
import statistics
import subprocess
import sys
import time
from collections import defaultdict

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent
TASK1 = ROOT / "Task1"
RUN = HERE / "run"
BUILD_CANDIDATES = [ROOT / "build", TASK1 / "build"]

RESULT_COLS = [
    "variant", "size", "threads", "block_size", "async_on", "rep", "total",
    "srm3", "srm5", "ela", "dct_inv", "dct_local", "dct_global", "wall_ms",
]

GENERATED = [
    "srm_3x3.png",
    "srm_5x5.png",
    "ela.png",
    "dct_invert.png",
    "dct_direct_local.png",
    "dct_direct_global.png",
    # Nombres antiguos: se limpian por compatibilidad con entregas previas.
    "srm_kernel_3x3.png",
    "srm_kernel_5x5.png",
    "dct_direct.png",
]


def cleanup_run_dir():
    RUN.mkdir(exist_ok=True)
    for name in GENERATED + ["one.csv"]:
        p = RUN / name
        if p.exists():
            p.unlink()


def binary_path(name: str) -> pathlib.Path:
    for build in BUILD_CANDIDATES:
        candidate = build / name
        if candidate.is_file():
            return candidate
    return BUILD_CANDIDATES[0] / name


def command_prefix(taskset_cpus, nice_level):
    prefix = []
    if nice_level is not None:
        prefix += ["nice", "-n", str(nice_level)]
    if taskset_cpus:
        prefix += ["taskset", "-c", taskset_cpus]
    return prefix


def run_one(binary: str, img: pathlib.Path, threads, async_on: bool, write_output: bool,
            block_size: int, taskset_cpus: str | None, nice_level: int | None):
    bp = binary_path(binary)
    if not bp.is_file():
        raise FileNotFoundError(
            f"No encuentro {bp}. Compila primero con: "
            "cmake -S . -B build && cmake --build build -j"
        )

    cleanup_run_dir()
    tmp_csv = RUN / "one.csv"

    args = command_prefix(taskset_cpus, nice_level) + [str(bp), str(img)]
    if threads is not None:
        args.append(str(threads))
    args += ["--block-size", str(block_size)]
    if not async_on:
        args.append("--no-async")
    if not write_output:
        args.append("--no-output")
    args += ["--csv", str(tmp_csv)]

    t0 = time.perf_counter()
    subprocess.run(args, cwd=str(RUN), stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL, check=True)
    wall_ms = int((time.perf_counter() - t0) * 1000)

    if not tmp_csv.exists():
        raise RuntimeError(f"{bp.name} no generó CSV de benchmark")

    with tmp_csv.open() as f:
        rec = list(csv.DictReader(f))[-1]

    # Compatibilidad: si se lee un CSV generado por una versión anterior, los dos
    # campos nuevos se derivan de dct_dir_ms.
    dct_local = rec.get("dct_local_ms", rec.get("dct_dir_ms", "0"))
    dct_global = rec.get("dct_global_ms", rec.get("dct_dir_ms", "0"))

    return {
        "srm3": int(rec["srm3_ms"]),
        "srm5": int(rec["srm5_ms"]),
        "ela": int(rec["ela_ms"]),
        "dct_inv": int(rec["dct_inv_ms"]),
        "dct_local": int(dct_local),
        "dct_global": int(dct_global),
        "total": int(rec["total_ms"]),
        "wall_ms": wall_ms,
    }


def summarize(rows, sizes, summary_path):
    groups = defaultdict(list)
    for r in rows:
        groups[(r["variant"], r["size"], r["threads"], r["block_size"])].append(r)

    seq_total_median = {}
    seq_total_min = {}
    for size in sizes:
        for block_size in sorted({r["block_size"] for r in rows if r["size"] == size}):
            seq_runs = groups.get(("seq", size, 1, block_size), [])
            if seq_runs:
                xs_seq = [int(r["total"]) for r in seq_runs]
                seq_total_median[(size, block_size)] = statistics.median(xs_seq)
                seq_total_min[(size, block_size)] = min(xs_seq)

    cols = ["variant", "size", "threads", "block_size", "reps", "median_ms", "mean_ms", "std_ms",
            "min_ms", "max_ms", "speedup_vs_seq_median", "speedup_vs_seq_best",
            "efficiency"]

    with open(summary_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        for (variant, size, threads, block_size), group in sorted(groups.items()):
            xs = [int(r["total"]) for r in group]
            median = statistics.median(xs)
            mean = statistics.mean(xs)
            std = statistics.stdev(xs) if len(xs) > 1 else 0.0
            seq_key = (size, block_size)
            speedup = (seq_total_median[seq_key] / median
                       if seq_key in seq_total_median and median > 0 else None)
            speedup_best = (seq_total_min[seq_key] / min(xs)
                            if seq_key in seq_total_min and min(xs) > 0 else None)

            if variant == "seq":
                p_eff = 1
            elif variant == "async_only":
                # Seis tareas funcionales: SRM3, SRM5, ELA, DCT inversa,
                # DCT directa local y DCT directa global.
                p_eff = 6
            elif variant == "hybrid":
                p_eff = 6 * max(1, threads // 6)
            else:
                p_eff = threads

            efficiency = speedup / p_eff if speedup else None
            w.writerow({
                "variant": variant,
                "size": size,
                "threads": threads,
                "block_size": block_size,
                "reps": len(xs),
                "median_ms": round(median, 2),
                "mean_ms": round(mean, 2),
                "std_ms": round(std, 2),
                "min_ms": min(xs),
                "max_ms": max(xs),
                "speedup_vs_seq_median": round(speedup, 3) if speedup else "",
                "speedup_vs_seq_best": round(speedup_best, 3) if speedup_best else "",
                "efficiency": round(efficiency, 3) if efficiency else "",
            })


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--reps", type=int, default=10)
    parser.add_argument("--warmup", type=int, default=2,
                        help="Repeticiones de calentamiento descartadas (caché fría, init runtime OpenMP).")
    parser.add_argument("--sizes", type=int, nargs="+", default=[256, 512, 1024])
    parser.add_argument("--threads", type=int, nargs="+", default=[1, 2, 4, 8],
                        help="Hilos OpenMP que se prueban. Default: 1 2 4 8.")
    parser.add_argument("--block-size", type=int, default=8, choices=[8, 16, 32],
                        help="Tamaño de bloque DCT usado por todos los binarios.")
    parser.add_argument("--image-dir", default=str(TASK1),
                        help="Directorio con imágenes test_<size>.png. Permite usar una carpeta con imágenes reales preparadas.")
    parser.add_argument("--include-hybrid", action="store_true",
                        help="Incluye detect (std::async + OpenMP). Puede ser variable si hay pocos núcleos.")
    parser.add_argument("--write-output", action="store_true",
                        help="Incluye la escritura final de PNG en cada ejecución.")
    parser.add_argument("--taskset-cpus", default=None,
                        help="Ejemplo: 0-7. Prefija cada ejecución con taskset -c para fijar afinidad CPU.")
    parser.add_argument("--nice-level", type=int, default=None,
                        help="Ejemplo: -10 en laboratorio si el usuario tiene permisos. Prefija con nice -n.")
    parser.add_argument("--out", default=str(HERE / "results.csv"))
    parser.add_argument("--summary", default=str(HERE / "results_summary.csv"))
    args = parser.parse_args()

    rows = []
    image_dir = pathlib.Path(args.image_dir)
    with open(args.out, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=RESULT_COLS)
        writer.writeheader()

        for size in args.sizes:
            img = image_dir / f"test_{size}.png"
            if not img.is_file():
                print(f"WARN: no existe {img}; se omite", file=sys.stderr)
                continue

            configs = [
                ("seq", "detect_seq", None, False),
                ("async_only", "detect_async", None, True),
            ]
            configs += [("omp", "detect_omp", t, False) for t in args.threads]
            if args.include_hybrid:
                configs += [("hybrid", "detect", t, True) for t in args.threads]

            # Warmup: ejecuciones descartadas para evitar el coste de carga
            # inicial del binario, caché fría de la imagen y arranque del
            # runtime OpenMP en la primera medida.
            for _ in range(max(0, args.warmup)):
                for variant, binary, threads, async_on in configs:
                    try:
                        run_one(binary, img, threads, async_on, args.write_output,
                                args.block_size, args.taskset_cpus, args.nice_level)
                    except Exception as exc:
                        print(f"WARN warmup {variant}@{size}: {exc}", file=sys.stderr)
            if args.warmup > 0:
                print(f"size={size} warmup x{args.warmup} done", flush=True)

            for rep in range(1, args.reps + 1):
                for variant, binary, threads, async_on in configs:
                    rec = run_one(binary, img, threads, async_on, args.write_output,
                                  args.block_size, args.taskset_cpus, args.nice_level)
                    row = {
                        "variant": variant,
                        "size": size,
                        "threads": threads if threads is not None else 1,
                        "block_size": args.block_size,
                        "async_on": int(async_on),
                        "rep": rep,
                        **rec,
                    }
                    writer.writerow(row)
                    f.flush()
                    rows.append(row)

                print(f"size={size} rep={rep} done", flush=True)

    summarize(rows, args.sizes, args.summary)
    cleanup_run_dir()
    print(f"Resultados: {args.out}")
    print(f"Resumen:    {args.summary}")


if __name__ == "__main__":
    main()
