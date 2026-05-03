#!/usr/bin/env bash
# Vuelca la información de la máquina paralela que el enunciado pide
# describir en la memoria (CPU, cachés, NUMA, OpenMP, compilador).
# Uso: bash scripts/cpu_info.sh > memoria/cpu_info.txt
set -u

echo "=========================================="
echo "  Caracterización de la máquina paralela"
echo "  $(date)"
echo "=========================================="
echo

if command -v lscpu >/dev/null 2>&1; then
    echo "--- lscpu ---"
    lscpu
    echo
fi

if [[ -r /proc/cpuinfo ]]; then
    echo "--- /proc/cpuinfo (resumen) ---"
    grep -m1 "model name" /proc/cpuinfo || true
    grep -m1 "cpu MHz" /proc/cpuinfo || true
    grep -m1 "cache size" /proc/cpuinfo || true
    echo "núm. cores lógicos visibles: $(grep -c ^processor /proc/cpuinfo)"
    if grep -m1 -q " ht " /proc/cpuinfo; then
        echo "flag 'ht' presente: SÍ — la CPU expone Hyper-Threading."
    elif grep -m1 -qw "ht" /proc/cpuinfo; then
        echo "flag 'ht' presente: SÍ — la CPU expone Hyper-Threading."
    else
        echo "flag 'ht' presente: NO."
    fi
    echo
fi

if command -v numactl >/dev/null 2>&1; then
    echo "--- numactl --hardware ---"
    numactl --hardware 2>/dev/null || true
    echo
fi

echo "--- Compilador C++ ---"
if command -v g++ >/dev/null 2>&1; then
    g++ --version | head -n 1
fi
if command -v clang++ >/dev/null 2>&1; then
    clang++ --version | head -n 1
fi
echo

echo "--- OpenMP (en tiempo de ejecución) ---"
TMP=$(mktemp -d)
cat > "$TMP/omp_probe.cc" <<'EOF'
#include <cstdio>
#include <omp.h>
int main(){
    #pragma omp parallel
    {
        #pragma omp master
        std::printf("omp_get_max_threads()=%d\nomp_get_num_procs()=%d\n_OPENMP=%d\n",
                    omp_get_max_threads(), omp_get_num_procs(), _OPENMP);
    }
    return 0;
}
EOF
if g++ -std=c++17 -fopenmp "$TMP/omp_probe.cc" -o "$TMP/omp_probe" 2>/dev/null; then
    "$TMP/omp_probe"
else
    echo "(no se pudo compilar el probe de OpenMP)"
fi
rm -rf "$TMP"
