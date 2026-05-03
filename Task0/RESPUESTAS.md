# Tarea 0 — Entrenamiento previo

Respuestas a las preguntas de la **Tarea 0** del enunciado de la Práctica 4.
Se mantienen los enunciados literales del PDF para que cada pregunta quede
emparejada con su respuesta. Las mismas respuestas (con más desarrollo y
figuras) aparecen también en `memoria/memoria.docx`.

---

## 0.1 OpenMP — `suma_vectores_float.cpp`

### 0.1.1 ¿Para qué sirve la variable `chunk`?

`chunk` es el **tamaño del bloque de iteraciones** que el planificador de
OpenMP entrega a cada hilo cada vez que reparte trabajo en un bucle
`#pragma omp for`. Se usa junto a la cláusula `schedule`:

- `schedule(static, chunk)` reparte iteraciones en bloques fijos de tamaño
  `chunk` de forma cíclica entre los hilos. Si no se indica `chunk`, el
  reparto se hace en bloques contiguos de tamaño `N/T`.
- `schedule(dynamic, chunk)` asigna bloques de tamaño `chunk` bajo demanda:
  cada hilo, al terminar el suyo, pide otro hasta que se agotan.
- `schedule(guided, chunk)` arranca con bloques grandes y los va
  reduciendo hasta el mínimo `chunk` indicado.

Un `chunk` pequeño mejora el balanceo (cuando las iteraciones tienen coste
variable) pero introduce overhead de sincronización; un `chunk` grande
reduce overhead pero empeora el balanceo si hay desequilibrios.

### 0.1.2 Explica completamente el pragma `#pragma omp parallel shared(a,b,c,chunk) private(i)`

`#pragma omp parallel` abre una **región paralela**: el hilo principal crea
un equipo de hilos (por defecto el número de cores disponibles, o lo que
indique `OMP_NUM_THREADS` o `omp_set_num_threads`) y todos ejecutan el
bloque que sigue. Al final de la región paralela hay una barrera implícita
y todos los hilos sincronizan antes de continuar.

Las cláusulas de visibilidad de variables son:

- **`shared(a,b,c,chunk)`** declara estas cuatro variables como
  **compartidas**: existe una sola copia y todos los hilos la ven. Se usa
  porque `a`, `b` y `c` son los vectores de entrada/salida del cálculo
  común, y cada hilo escribe en posiciones distintas de `c[i]`, por lo que
  no hay condición de carrera. `chunk` es una constante de configuración
  que todos los hilos consultan; tenerla compartida ahorra memoria y es
  consistente.

- **`private(i)`** declara `i` como **privada**: cada hilo tiene su propia
  copia de `i`. Se hace así porque `i` es la **variable inductora** del
  bucle y cada hilo recorre un rango distinto. Si fuera compartida los
  hilos se pisarían el contador y el resultado sería incorrecto. En OpenMP
  la variable inductora del `for` paralelizado ya es privada por defecto,
  pero declararla explícitamente es buena práctica de claridad.

### 0.1.3 ¿Para qué sirve `schedule`? ¿Qué otras posibilidades hay?

`schedule` decide **cómo se reparten las iteraciones** del `for` paralelo
entre los hilos. Las posibilidades estándar son:

| Tipo | Comportamiento | Cuándo usarla |
|------|----------------|---------------|
| `static` | Reparto fijo en tiempo de compilación, en bloques contiguos de tamaño `N/T` (o `chunk` si se indica) | Iteraciones con coste homogéneo |
| `dynamic` | Reparto bajo demanda en bloques de tamaño `chunk` | Iteraciones con coste muy variable |
| `guided` | Bloques grandes que decrecen hasta `chunk` | Cargas variables con tendencia decreciente |
| `runtime` | Se decide en tiempo de ejecución vía la variable de entorno `OMP_SCHEDULE` | Comparativas o ajuste fino sin recompilar |
| `auto` | El compilador/runtime elige | Cuando no hay criterio claro |

Para una suma vectorial (como la del programa) el coste por iteración es
constante, por lo que `static` (sin chunk) es la mejor opción: minimiza el
overhead de planificación.

### 0.1.4 ¿Qué tiempos y otras medidas de rendimiento podemos medir en secciones de código paralelizadas con OpenMP?

OpenMP ofrece varias formas de instrumentar y medir rendimiento:

- **Tiempo de pared (wall time)** con `omp_get_wtime()`, que devuelve
  segundos en doble precisión y tiene resolución alta (`omp_get_wtick()`).
  Es la métrica más útil para calcular speed-up y eficiencia. También es
  válido `std::chrono::steady_clock` desde C++.
- **Número de hilos efectivamente usados**: `omp_get_num_threads()` dentro
  de la región paralela, `omp_get_max_threads()` antes de entrar.
- **Identificador del hilo**: `omp_get_thread_num()` para depurar y para
  registros de instrumentación por hilo.
- **Speed-up**: `S(p) = T(1) / T(p)` comparando la versión secuencial con
  la versión paralela ejecutada con `p` hilos.
- **Eficiencia**: `E(p) = S(p) / p`. Cuanto más cerca de 1, mejor uso de
  los recursos.
- **Coste de overhead** (creación de hilos, sincronización, balanceo). Se
  puede aproximar comparando la versión secuencial directa con la versión
  OpenMP ejecutada con un único hilo: `T_overhead ≈ T_omp(1) - T_seq`.
- **Profilers externos**: `perf stat`, `gprof`, `Intel VTune`, `Linux
  Perf`, `omp-tools (OMPT)` permiten medir IPC, fallos de caché, ciclos
  por hilo, *false sharing*, etc., complementando los tiempos.

En la práctica medimos `wall time` por etapa y total, calculamos
`speed-up` y `eficiencia` para distintas tallas y números de hilos, y los
exponemos en `benchmarks/results_summary.csv` y en las gráficas de la
memoria.

---

## 0.2 std::async — `async_functions.cpp`

### 0.2.1 ¿Para qué sirve el parámetro `std::launch::async`?

`std::launch::async` es la **política de lanzamiento** que se pasa a
`std::async` para garantizar que la función se ejecuta en un **hilo
distinto** al que llama. Sin esa política el comportamiento queda en
manos del runtime: puede decidir ejecutar la tarea de forma diferida
(en el hilo que llame a `.get()`) y entonces no hay paralelismo real.

En resumen: `std::launch::async` fuerza ejecución concurrente en otro
hilo. Es la política que debe usarse cuando se quiere paralelizar de
verdad.

### 0.2.2 Calcula el tiempo que tarda el programa con `std::launch::async` y `std::launch::deferred`. ¿A qué se debe la diferencia de tiempos?

El programa lanza dos tareas que duermen 2000 ms y 3000 ms.

- Con **`std::launch::async`**, ambas tareas se ejecutan en hilos
  distintos en paralelo. El tiempo total es aproximadamente el máximo de
  ambas: **~3000 ms** (3 segundos).
- Con **`std::launch::deferred`**, no se crea ningún hilo nuevo: la tarea
  queda diferida y solo se ejecuta cuando se llama a `wait()` o `get()`,
  y entonces se ejecuta en el hilo principal. El tiempo total es la
  **suma** de ambas, **~5000 ms** (5 segundos), porque se ejecutan en
  serie.

La diferencia (~2000 ms) corresponde al solapamiento que se gana al
ejecutar las tareas concurrentemente. Es el ejemplo canónico de
paralelismo funcional: dos cálculos independientes pueden hacerse a la
vez si hay cores disponibles.

### 0.2.3 ¿Qué diferencia hay entre los métodos `wait` y `get` de `std::future`?

Ambos bloquean al hilo llamante hasta que la tarea asociada al `future`
finaliza, pero con dos diferencias:

- **`get()`** devuelve el **valor de retorno** de la tarea (o relanza la
  excepción que la tarea haya lanzado). Solo puede llamarse **una vez**:
  tras hacerlo el `future` queda en estado no válido.
- **`wait()`** **no devuelve nada**: solo espera a que la tarea termine.
  Puede llamarse varias veces mientras el `future` siga siendo válido,
  pero no debe usarse el mismo `std::future` concurrentemente desde varios
  hilos sin sincronización externa; para eso existe `std::shared_future`.
  Existen también `wait_for(...)` y `wait_until(...)` para esperas con
  timeout.

En el programa de la Tarea 0.2 se usan los dos: `task1.wait()` para
esperar la primera y `task2.get()` para esperar la segunda y recoger el
resultado.

### 0.2.4 ¿Qué ventajas ofrece `std::async` frente a `std::thread`?

- **Devuelve el resultado**: `std::async` retorna un `std::future<T>` que
  permite obtener el valor de retorno de la función. `std::thread` no
  devuelve nada; hay que orquestar variables compartidas o promesas a
  mano.
- **Propaga excepciones**: si la tarea lanza, la excepción se captura en
  el `future` y se relanza en `get()`. Con `std::thread`, una excepción
  no manejada termina el programa con `std::terminate`.
- **Gestión automática del ciclo de vida**: no hay que llamar a `join()`
  ni `detach()`; el destructor del `future` se ocupa.
- **Menos código de sincronización manual**: no hay que crear una promesa,
  guardar el resultado en memoria compartida ni coordinar explícitamente
  la recogida del valor.
- **Política configurable**: con `std::launch::async`, `deferred` o la
  combinación de ambas se indica al runtime si se quiere ejecución
  concurrente, diferida o una elección automática según la implementación.
- **API más declarativa**: el código expresa el "qué" (calcular esto en
  paralelo) en lugar del "cómo" (crear este hilo, sincronizarlo).

`std::thread` sigue siendo útil cuando se necesita control fino del hilo
(p. ej. afinidad, prioridad, ciclos largos de ejecución desligados de un
resultado).

---

## 0.3 std::vector — `initialize_vectors.cpp`

### 0.3.1 ¿Cuál de las dos formas de inicializar el vector y rellenarlo es más eficiente? ¿Por qué?

**La inicialización con tamaño y acceso directo (`std::vector<float> v(N); v[i] = ...`) es claramente más eficiente** que la basada en `push_back` sobre un vector vacío.

Razones:

- `std::vector` mantiene una `capacity` interna. Cuando se hace
  `push_back` y la capacidad se agota, **reasigna memoria** (típicamente
  duplica la capacidad), copia/mueve los elementos existentes y libera el
  bloque anterior. En un bucle de N inserciones esto produce O(log N)
  reasignaciones, cada una con coste proporcional al tamaño actual.
- Crear el vector con tamaño N (`std::vector<float> v(N)`) reserva la
  memoria una sola vez y construye los N elementos con su valor por
  defecto. A partir de ahí, el bucle accede por índice `v[i] = x` que es
  una escritura directa a memoria contigua, igual de rápida que un array
  C.
- También es posible `v.reserve(N); v.push_back(...)`, que evita la
  inicialización por defecto y mantiene la API de `push_back` sin
  reasignaciones; suele ser aún ligeramente más rápida.

En los tiempos del programa de ejemplo se aprecia claramente: la versión
con `push_back` es varias veces más lenta que la versión con acceso
directo, especialmente al aumentar `N`.

### 0.3.2 ¿Podría ocurrir algún problema al paralelizar los dos bucles for? ¿Por qué?

- **Bucle 1 (`v1.push_back(i)`)**: **NO se puede paralelizar tal cual**.
  `push_back` modifica internamente el tamaño y, potencialmente, el
  puntero al buffer si hay reasignación. Es una operación con estado
  compartido y **no es thread-safe** sin sincronización explícita. Si
  varios hilos hacen `push_back` a la vez se producen carreras de datos,
  corrupción de la estructura interna del vector y, con alta
  probabilidad, fallos de segmentación. Para paralelizarlo habría que
  reservar tamaño con `resize(N)` y escribir por índice.

- **Bucle 2 (`v2[i] = i`)**: **SÍ es paralelizable**. Cada iteración
  escribe en una posición distinta de un buffer ya reservado, por lo que
  no hay condición de carrera. Bastaría un `#pragma omp parallel for
  schedule(static)`. Hay que tener en cuenta dos detalles secundarios:
  - **First-touch en sistemas NUMA**: la primera escritura sobre una
    página la asigna físicamente al socket que la realiza. Si la
    inicialización se paraleliza, las páginas quedan repartidas entre
    sockets, lo cual luego beneficia los bucles paralelos que las leen.
  - **Falso compartido (false sharing)** si varios hilos escriben en
    posiciones distintas que caen dentro de la misma cache line. Con
    `schedule(static)` y bloques contiguos grandes por hilo el efecto suele
    ser pequeño o despreciable.

---

## Compilación

```bash
cd Task0
g++ -O2 -fopenmp suma_vectores_float.cpp -o suma_vectores_float
g++ -O2 -std=c++17 async_functions.cpp -pthread -o async_functions
g++ -O2 -std=c++17 initialize_vectors.cpp -o initialize_vectors

./suma_vectores_float
./async_functions
./initialize_vectors
```

OpenMP requiere `-fopenmp` tanto en compilación como en enlazado.
`std::async` requiere enlazar con la librería de hilos: `-pthread` (o
`-lpthread`).
