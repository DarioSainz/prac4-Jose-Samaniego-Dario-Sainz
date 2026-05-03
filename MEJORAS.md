# MEJORAS / CHANGELOG respecto al `sourceP3` original

## 1. Benchmark más defendible

- `benchmarks/bench.py` ahora usa por defecto `--threads 1 2 4 8`.
- Todas las tallas usan `--reps 10` por defecto.
- Se añade `--warmup 2` para descartar arranques en frío.
- Se añade `--taskset-cpus` para fijar afinidad CPU, por ejemplo `--taskset-cpus 0-7`.
- Se añade `--nice-level` para ejecutar con prioridad controlada si el sistema lo permite.
- Se mantiene `--no-output` como comportamiento por defecto del benchmark para medir cómputo y no E/S de disco.

## 2. Verificación de equivalencia entre variantes

- `scripts/verify_build.sh` ya no comprueba solo que existan PNGs.
- Ahora compila y ejecuta `detect_seq`, `detect_async`, `detect_omp` y `detect` sobre la misma imagen.
- Compara las salidas contra la versión secuencial con equivalencia bit-a-bit.
- Si el binario PNG difiere, hace comparación píxel-a-píxel con tolerancia `EPS` mediante Pillow.
- Esto protege contra regresiones silenciosas por condiciones de carrera o cambios de normalización.

## 3. Imagen fotográfica manipulada

- Se añade `Task1/images/astronaut_faceswap_512.png`.
- La imagen parte de una fotografía real de ejemplo (`skimage.data.astronaut`) y contiene una manipulación local visible en la región facial.
- Se añade `Task1/images/README.md` explicando qué debería mostrar cada salida: SRM, ELA, DCT inversa, DCT directa local y DCT directa global.

## 4. Fuga de memoria en `Image<T>`

- `Image<T>::matrix` usa `std::shared_ptr<T[]>`.
- Se evita el patrón peligroso `new[]` sin `delete[]`.
- `release()` libera la referencia, y la memoria se recupera automáticamente cuando no quedan propietarios.
- El diseño permite copias baratas y evita fugas al devolver imágenes temporales desde operadores y funciones.

## 5. DCT directa: normalización local y global

- Antes se normalizaba por bloque y después globalmente, mezclando dos criterios.
- Ahora se generan dos salidas separadas:
  - `dct_direct_local.png`: normalización por bloque, útil para visualización local.
  - `dct_direct_global.png`: normalización global, útil para comparar energía relativa entre regiones.
- La memoria/README explica por qué la salida global es más correcta para razonar sobre energía entre zonas.

## 6. `block_size` configurable

- El tamaño de bloque DCT deja de estar hardcoded a 8.
- Se añade CLI:

```bash
--block-size 8|16|32
```

- Esto permite probar sensibilidad de la DCT con bloques 8, 16 y 32 sin tocar el código.

## 7. Gráfica Amdahl teórico vs medido

- `benchmarks/plot.py` genera `memoria/figures/fig6_amdahl_teorico_vs_medido.png`.
- Se estima la fracción no paralelizable `f` a partir de los puntos OpenMP.
- Se dibuja `S(p) = 1 / (f + (1-f)/p)` y se superponen los puntos medidos.

## 8. Memoria y defensa Tarea 1.3

- Se añade contenido explícito con la terminología pedida:
  - tipos de paralelismo usado (datos / funcional / híbrido)
  - modo de programación paralela (variables compartidas)
  - alternativas de comunicación (implícitas vía memoria compartida)
  - estilo de programación paralela (SPMD para OpenMP, fork-join para std::async)
  - tipo de estructura paralela (paralelismo de bucles + paralelismo funcional)

## 9. Limpieza del entregable

- Se recupera `ENTREGA_CHECKLIST.md`.
- Se añade este `MEJORAS.md` para que el profesor vea rápido el delta.
- Se actualiza `.gitignore` con los nuevos nombres de salidas.
- Se documentan `--no-output`, `--block-size`, la imagen manipulada y la verificación de equivalencia.

## 10. Sustitución de mediciones de VM por medición física en portátil

Se eliminan los resultados antiguos de la VM que mostraban `Model name: unknown`, 56 CPUs lógicas virtualizadas y desviaciones estándar excesivas. En su lugar se incluyen CSV y figuras obtenidos/documentados para la máquina física indicada por el usuario:

- AMD Ryzen 9 7845HX with Radeon Graphics.
- 12 núcleos físicos y 24 hilos lógicos.
- 16 GB de RAM comerciales / 15.21 GiB reportados.
- Ubuntu en portátil enchufado.
- Benchmarks con T = 1, 2, 4, 8, `--reps 10`, `--warmup 2`, `--block-size 8` y `--no-output`.

Estos datos quedan documentados como **medición física en portátil** en `benchmarks/README_MEDICIONES.md` y `memoria/cpu_info.txt`. Para reproducir la medición, ejecutar:

```bash
bash scripts/cpu_info.sh > memoria/cpu_info.txt
cd benchmarks
python3 bench.py --reps 10 --warmup 2 --sizes 256 512 1024 --threads 1 2 4 8 --block-size 8 --taskset-cpus 0-7 --nice-level -10 --include-hybrid
python3 plot.py
```

en el portátil o en una máquina equivalente del laboratorio.
