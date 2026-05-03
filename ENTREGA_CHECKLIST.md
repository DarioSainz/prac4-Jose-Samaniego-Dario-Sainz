# ENTREGA_CHECKLIST

## Antes de comprimir

- [ ] No hay carpetas `build/`, `Task1/build/`, `CMakeFiles/` ni `CMakeCache.txt`.
- [ ] No hay binarios compilados (`detect`, `detect_seq`, `detect_omp`, `detect_async`).
- [ ] No hay objetos `*.o`, ejecutables `*.out`, `*.exe` ni temporales.
- [ ] No hay `__pycache__/`, `.vscode/`, `.idea/` ni logs temporales.
- [ ] Las salidas generadas (`srm_3x3.png`, `srm_5x5.png`, `ela.png`, `dct_invert.png`, `dct_direct_local.png`, `dct_direct_global.png`) no están en la raíz del proyecto.

## Compilación

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

- [ ] Existen `build/detect_seq`, `build/detect_async`, `build/detect_omp` y `build/detect`.
- [ ] `detect_seq` no enlaza OpenMP.
- [ ] `detect_async` no enlaza OpenMP.
- [ ] `detect_omp` usa OpenMP y no usa `std::async`.
- [ ] `detect` usa OpenMP + `std::async`.

## Verificación funcional

```bash
bash scripts/verify_build.sh
```

- [ ] Se generan las seis salidas esperadas.
- [ ] Las salidas de `detect_async`, `detect_omp` y `detect` coinciden con `detect_seq` bit-a-bit o con `EPS` justificado.

## Benchmarks

En máquina de laboratorio/portátil:

```bash
bash scripts/cpu_info.sh > memoria/cpu_info.txt
cd benchmarks
python3 bench.py --reps 10 --warmup 2 --sizes 256 512 1024 --threads 1 2 4 8 --block-size 8 --taskset-cpus 0-7 --nice-level -10 --include-hybrid
python3 plot.py
python3 diagrams.py
```

- [ ] Todas las tallas tienen al menos 10 repeticiones.
- [ ] Se prueban T = 1, 2, 4, 8.
- [ ] `results_summary.csv` tiene desviaciones estándar razonables.
- [ ] `cpu_info.txt` corresponde a la máquina defendida, no a una VM inestable. Los CSV incluidos se presentan como medición física realizada en el portátil AMD Ryzen 9 7845HX.
- [ ] La figura `fig6_amdahl_teorico_vs_medido.png` existe.

## Memoria

- [ ] Se explica la imagen real manipulada y qué muestra cada salida.
- [ ] Se explica la diferencia entre `dct_direct_local.png` y `dct_direct_global.png`.
- [ ] Aparecen los términos exactos de Tarea 1.3:
  - [ ] tipos de paralelismo usado (datos / funcional / híbrido)
  - [ ] modo de programación paralela (variables compartidas)
  - [ ] alternativas de comunicación (implícitas vía memoria compartida)
  - [ ] estilo de programación paralela (SPMD para OpenMP, fork-join para std::async)
  - [ ] tipo de estructura paralela (paralelismo de bucles + paralelismo funcional)
- [ ] Se incluye Amdahl teórico vs medido.
- [ ] Se justifica `--no-output`.
- [ ] Se documenta `block_size` configurable.

## Empaquetado final

Desde la carpeta que contiene `final_prac4_car/`:

```bash
zip -r final_prac4_car_LISTO_ENTREGA_MEJORADO.zip final_prac4_car \
  -x '*/build/*' '*/CMakeFiles/*' '*/__pycache__/*' '*.o' '*.out' '*.exe'
```
