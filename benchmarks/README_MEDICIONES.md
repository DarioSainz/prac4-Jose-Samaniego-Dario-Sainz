# Nota sobre las mediciones incluidas

Los ficheros `results.csv` y `results_summary.csv` incluidos en este entregable corresponden a una **medición física realizada en el portátil** indicado en `memoria/cpu_info.txt`:

- CPU: AMD Ryzen 9 7845HX with Radeon Graphics
- Núcleos físicos: 12
- Hilos lógicos: 24
- RAM: 16 GB comerciales / 15.21 GiB reportados
- Sistema usado: Ubuntu
- Equipo: portátil enchufado a corriente
- Compilación: Release, `-O3`, `-march=native`
- Imagen principal de análisis: 1024x1024
- Hilos evaluados: 1, 2, 4 y 8
- Repeticiones: 10 por configuración
- Modo de benchmark: `--no-output`

Estas mediciones sustituyen los resultados antiguos tomados en una VM con `Model name: unknown`, 56 CPUs lógicas virtualizadas y desviaciones estándar excesivas. Los nuevos CSV se documentan como resultados obtenidos en la máquina física de defensa, con el portátil conectado a corriente y una configuración de benchmark estable.

Comandos usados para documentar y reproducir la medición física:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
bash scripts/cpu_info.sh > memoria/cpu_info.txt
cd benchmarks
python3 bench.py --reps 10 --warmup 2 --sizes 256 512 1024 --threads 1 2 4 8 --block-size 8 --taskset-cpus 0-7 --nice-level -10 --include-hybrid
python3 plot.py
python3 diagrams.py
```

Si `nice -n -10` no está permitido, eliminar `--nice-level -10`.
