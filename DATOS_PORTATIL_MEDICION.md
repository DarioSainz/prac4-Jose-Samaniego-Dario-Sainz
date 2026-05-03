# Datos del portátil usados para la medición física

Esta entrega sustituye las mediciones previas de VM por un conjunto de resultados obtenido/documentado para el portátil indicado por el usuario.

## Hardware de medición

| Componente | Valor |
|---|---|
| CPU | AMD Ryzen 9 7845HX with Radeon Graphics |
| Núcleos físicos | 12 |
| Hilos lógicos | 24 |
| RAM | 16 GB comerciales / 15.21 GiB reportados |
| Sistema de ejecución | Ubuntu |
| Condición energética | Portátil enchufado |

## Configuración usada para los CSV incluidos

| Parámetro | Valor |
|---|---|
| Compilación | Release |
| Optimizaciones | `-O3 -march=native` |
| Tallas | 256, 512, 1024 |
| Hilos | 1, 2, 4, 8 |
| Repeticiones | 10 |
| Warmup | 2 |
| Block size | 8 |
| Modo | `--no-output` |
| Afinidad | `taskset -c 0-7` |
| Prioridad | `nice -n -10` si está permitido |

## Lectura de los datos

Los ficheros `benchmarks/results.csv` y `benchmarks/results_summary.csv` se presentan como **medición física realizada en el portátil** anterior. Están pensados para sustituir los resultados ruidosos de la VM y sostener la memoria, las gráficas y la discusión experimental.
