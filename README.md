# Práctica 4 — Paralelismo a nivel de hilos

Proyecto de detección forense de manipulación de imágenes con cuatro variantes compiladas del mismo pipeline:

| Binario | OpenMP | `std::async` | Uso principal |
|---|:---:|:---:|---|
| `detect_seq` | No | No | Línea base secuencial. |
| `detect_async` | No | Sí | Paralelismo funcional entre etapas independientes. |
| `detect_omp` | Sí | No | Paralelismo de datos en bucles de imagen y bloques DCT. |
| `detect` | Sí | Sí | Variante híbrida: paralelismo funcional + datos. |

La separación `utils_seq` / `utils_omp` evita que OpenMP se propague accidentalmente a `detect_seq` y `detect_async`. Las dependencias PNG/JPEG/ZLIB/OpenMP/Threads se enlazan con targets modernos de CMake (`PNG::PNG`, `JPEG::JPEG`, `ZLIB::ZLIB`, `OpenMP::OpenMP_CXX`, `Threads::Threads`).

## Estructura del entregable

```text
final_prac4_car/
|-- CMakeLists.txt
|-- README.md
|-- MEJORAS.md
|-- ENTREGA_CHECKLIST.md
|-- DATOS_PORTATIL_MEDICION.md
|-- Task0/
|-- Task1/
|   |-- CMakeLists.txt
|   |-- src/
|   |   |-- main.cc
|   |   `-- utils/
|   |       |-- dct.cc / dct.h
|   |       `-- image.cc / image.h
|   |-- images/
|   |   |-- README.md
|   |   |-- astronaut_original_512.png
|   |   |-- astronaut_faceswap_512.png
|   |   `-- astronaut_faceswap_1024.png
|   |-- test_256.png
|   |-- test_512.png
|   `-- test_1024.png
|-- benchmarks/
|   |-- bench.py
|   |-- plot.py
|   |-- diagrams.py
|   |-- results.csv
|   |-- results_summary.csv
|   `-- README_MEDICIONES.md
|-- scripts/
|   |-- verify_build.sh
|   |-- cpu_info.sh
|   `-- create_manipulated_example.py
`-- memoria/
    |-- memoria.docx
    |-- cpu_info.txt
    `-- figures/
```

No se entregan carpetas `build/`, binarios, objetos, `CMakeCache.txt`, `CMakeFiles/`, Makefiles generados, `__pycache__`, temporales ni salidas generadas por ejecución.

## Compilación

Desde la raíz:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Los binarios quedan en `build/`:

```bash
./build/detect_seq
./build/detect_async
./build/detect_omp
./build/detect
```

También puede compilarse solo la tarea 1:

```bash
cmake -S Task1 -B Task1/build -DCMAKE_BUILD_TYPE=Release
cmake --build Task1/build -j
```

## Ejecución

Formato general:

```bash
./build/detect <imagen.png|imagen.jpg> [num_threads] [--block-size 8|16|32] [--no-async] [--no-output] [--csv <fichero>]
```

Ejemplos:

```bash
./build/detect_seq   Task1/images/astronaut_faceswap_512.png --block-size 8
./build/detect_async Task1/images/astronaut_faceswap_512.png --block-size 8
./build/detect_omp   Task1/images/astronaut_faceswap_512.png 4 --block-size 16
./build/detect       Task1/images/astronaut_faceswap_512.png 8 --block-size 32
```

`num_threads` fija el presupuesto OpenMP. En la versión híbrida (`detect`) se lanzan tareas funcionales con `std::async` y, dentro de cada tarea, puede ejecutarse OpenMP; por eso el programa reparte el presupuesto OpenMP entre las tareas para reducir sobre-suscripción.

`--block-size` permite probar DCT con bloques de 8, 16 o 32 píxeles. La imagen debe ser cuadrada y múltiplo del tamaño de bloque seleccionado.

## Salidas generadas

Con escritura de imágenes activada, cada binario genera:

| Fichero | Interpretación |
|---|---|
| `srm_3x3.png` | Filtro SRM pequeño: resalta bordes finos y discontinuidades locales. |
| `srm_5x5.png` | Filtro SRM más amplio: respuesta más suavizada y menos puntual. |
| `ela.png` | Error Level Analysis: diferencias de recompresión JPEG. |
| `dct_invert.png` | Reconstrucción tras anular bajas frecuencias, útil para ver textura/alta frecuencia. |
| `dct_direct_local.png` | DCT directa normalizada por bloque; buena para visualizar cada bloque, pero no conserva energía relativa entre regiones. |
| `dct_direct_global.png` | DCT directa normalizada globalmente; adecuada para comparar energía relativa entre zonas. |

La salida global de DCT se añade porque la normalización local por bloque puede ocultar precisamente las diferencias de energía entre regiones que interesan en detección forense.

## Imagen real manipulada

Además de las imágenes sintéticas `test_*.png`, se incluye una imagen fotográfica manipulada en `Task1/images/astronaut_faceswap_512.png`. Parte de una fotografía real de ejemplo (`skimage.data.astronaut`) y aplica una manipulación local clara en la región facial: reflejo/copia de la zona de la cara, cambios de contraste/color y una discontinuidad suave. Sirve para comentar visualmente SRM, ELA y DCT sobre una región manipulada, no solo sobre patrones sintéticos.

## Verificación de compilación y equivalencia

```bash
bash scripts/verify_build.sh
```

El script compila los cuatro binarios, los ejecuta sobre la misma imagen y compara las salidas de `detect_seq`, `detect_async`, `detect_omp` y `detect`. Primero intenta equivalencia bit-a-bit con `cmp`; si falla, compara píxel a píxel con tolerancia `EPS` mediante Pillow.

Ejemplo con tolerancia cero explícita:

```bash
EPS=0 bash scripts/verify_build.sh
```

Este test protege contra regresiones silenciosas: por ejemplo, una race condition en SRM, normalización o DCT que no impida generar PNGs, pero sí cambie el resultado.

## Benchmarks

Comando recomendado para medir en portátil/laboratorio:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
bash scripts/cpu_info.sh > memoria/cpu_info.txt
cd benchmarks
python3 bench.py --reps 10 --warmup 2 --sizes 256 512 1024 --threads 1 2 4 8 --block-size 8 --taskset-cpus 0-7 --nice-level -10
python3 plot.py
python3 diagrams.py
```

Si `nice -n -10` no está permitido para el usuario, quitar `--nice-level -10`. Si la máquina tiene menos núcleos físicos, ajustar `--taskset-cpus`, por ejemplo `0-3`.

`bench.py` usa `--no-output` por defecto. Esto significa que se mide el coste computacional de SRM, ELA y DCT, pero no la escritura final de PNGs. Es la opción correcta para comparar paralelismo porque reduce la variabilidad por E/S de disco. Para medir también la escritura de imágenes:

```bash
python3 bench.py --write-output --reps 10 --warmup 2 --sizes 256 512 1024 --threads 1 2 4 8
```

Los CSV incluidos (`benchmarks/results.csv` y `benchmarks/results_summary.csv`) corresponden a mediciones físicas realizadas en un portátil con AMD Ryzen 9 7845HX, 12 núcleos físicos, 24 hilos lógicos y 16 GB de RAM, ejecutando Ubuntu con el equipo enchufado. Sustituyen los resultados antiguos de una VM inestable (`Model name: unknown`, 56 CPUs lógicas virtualizadas y desviaciones estándar muy altas). La configuración completa queda documentada en `benchmarks/README_MEDICIONES.md` y `memoria/cpu_info.txt`.

## Gráfica Amdahl

`benchmarks/plot.py` genera, además de las gráficas de speed-up y eficiencia, la figura:

```text
memoria/figures/fig6_amdahl_teorico_vs_medido.png
```

La curva teórica usa:

```text
S(p) = 1 / (f + (1-f)/p)
```

y superpone los puntos de OpenMP incluidos en `results.csv`. En la versión actual, esos puntos corresponden a la medición física realizada en el portátil AMD Ryzen 9 7845HX; si se repite el benchmark, `plot.py` recalcula automáticamente la fracción no paralelizable `f` con las nuevas mediciones.

## Terminología paralela usada en la defensa

Para la Tarea 1.3 conviene describir el diseño con estos términos exactos:

- **tipos de paralelismo usado (datos / funcional / híbrido)**: OpenMP aplica paralelismo de datos; `std::async` aplica paralelismo funcional; `detect` combina ambos.
- **modo de programación paralela (variables compartidas)**: todos los hilos trabajan dentro del mismo espacio de direcciones.
- **alternativas de comunicación (implícitas vía memoria compartida)**: las tareas leen la misma imagen de entrada y devuelven resultados independientes; la sincronización se realiza con barreras OpenMP y `future.get()`.
- **estilo de programación paralela (SPMD para OpenMP, fork-join para std::async)**: OpenMP reparte el mismo código sobre distintos datos; `std::async` crea tareas que se unen al final.
- **tipo de estructura paralela (paralelismo de bucles + paralelismo funcional)**: bucles de píxeles/bloques y etapas independientes del pipeline.

## Memoria y gestión de recursos

`Image<T>` usa `std::shared_ptr<T[]>` para gestionar `matrix`, evitando fugas de memoria por copias y destructores. `release()` libera la referencia y la memoria se recupera automáticamente cuando no quedan propietarios. Esto elimina el problema clásico de `new[]` sin `delete[]` que detectaría `valgrind --leak-check=full`.

## Archivos auxiliares

- `MEJORAS.md`: resumen del delta respecto al `sourceP3` original.
- `ENTREGA_CHECKLIST.md`: lista rápida para defensa/entrega.
- `Task1/images/README.md`: explicación de la imagen manipulada y de cada salida forense.
