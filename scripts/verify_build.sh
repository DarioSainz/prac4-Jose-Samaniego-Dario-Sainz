#!/usr/bin/env bash
# Verificación rápida de entrega:
#   1) Compila los cuatro binarios.
#   2) Ejecuta cada variante sobre la misma imagen.
#   3) Comprueba que se generan las salidas esperadas.
#   4) Comprueba equivalencia entre variantes: los PNGs deben coincidir bit-a-bit
#      o, si el codificador PNG introduce diferencias no semánticas, con tolerancia
#      píxel-a-píxel EPS mediante Pillow.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

EPS="${EPS:-0}"
TEST_IMAGE="${TEST_IMAGE:-$ROOT/Task1/images/astronaut_faceswap_512.png}"
if [[ ! -f "$TEST_IMAGE" ]]; then
    TEST_IMAGE="$ROOT/Task1/test_256.png"
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc 2>/dev/null || echo 2)"

for exe in detect_seq detect_async detect_omp detect; do
    test -x "build/$exe"
    echo "OK: build/$exe"
done

EXPECTED_OUTPUTS=(
    srm_3x3.png
    srm_5x5.png
    ela.png
    dct_invert.png
    dct_direct_local.png
    dct_direct_global.png
)

run_variant() {
    local label="$1"; shift
    local tmp="$1"; shift
    mkdir -p "$tmp"
    (
        cd "$tmp"
        "$@" >/dev/null
    )
    for f in "${EXPECTED_OUTPUTS[@]}"; do
        if [[ ! -s "$tmp/$f" ]]; then
            echo "FALLO: $label no generó $f" >&2
            return 1
        fi
    done
    echo "OK ejecución: $label"
}

compare_with_tolerance() {
    local ref="$1"
    local got="$2"
    local eps="$3"
    python3 - "$ref" "$got" "$eps" <<'PY'
from pathlib import Path
import sys
try:
    from PIL import Image
except Exception as exc:
    print(f"Pillow no disponible para comparación tolerante: {exc}", file=sys.stderr)
    sys.exit(2)

ref, got, eps = Path(sys.argv[1]), Path(sys.argv[2]), int(sys.argv[3])
a = Image.open(ref).convert("RGBA")
b = Image.open(got).convert("RGBA")
if a.size != b.size:
    print(f"tamaño distinto: {a.size} != {b.size}", file=sys.stderr)
    sys.exit(1)
pa = a.tobytes()
pb = b.tobytes()
maxdiff = max(abs(x-y) for x, y in zip(pa, pb)) if pa else 0
if maxdiff > eps:
    print(f"maxdiff={maxdiff} > EPS={eps}", file=sys.stderr)
    sys.exit(1)
PY
}

compare_outputs() {
    local ref_dir="$1"
    local got_dir="$2"
    local label="$3"
    for f in "${EXPECTED_OUTPUTS[@]}"; do
        if cmp -s "$ref_dir/$f" "$got_dir/$f"; then
            echo "OK equivalencia bit-a-bit: $label/$f"
        else
            compare_with_tolerance "$ref_dir/$f" "$got_dir/$f" "$EPS"
            echo "OK equivalencia tolerante EPS=$EPS: $label/$f"
        fi
    done
}

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

run_variant "detect_seq"   "$TMP_ROOT/seq"   "$ROOT/build/detect_seq"   "$TEST_IMAGE" --block-size 8
run_variant "detect_async" "$TMP_ROOT/async" "$ROOT/build/detect_async" "$TEST_IMAGE" --block-size 8
run_variant "detect_omp"   "$TMP_ROOT/omp"   "$ROOT/build/detect_omp"   "$TEST_IMAGE" 2 --block-size 8
run_variant "detect"       "$TMP_ROOT/hyb"   "$ROOT/build/detect"       "$TEST_IMAGE" 2 --block-size 8

compare_outputs "$TMP_ROOT/seq" "$TMP_ROOT/async" "detect_async"
compare_outputs "$TMP_ROOT/seq" "$TMP_ROOT/omp"   "detect_omp"
compare_outputs "$TMP_ROOT/seq" "$TMP_ROOT/hyb"   "detect"

echo
echo "Verificación completada correctamente. Imagen usada: $TEST_IMAGE"
echo "EPS=$EPS (0 = equivalencia exacta salvo fallback sin diferencia píxel-a-píxel)."
