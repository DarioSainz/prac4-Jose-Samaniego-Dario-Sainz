# Imagen real manipulada para la defensa forense

Se incluye `astronaut_original_512.png` y `astronaut_faceswap_512.png` como ejemplo visual más realista que las imágenes sintéticas `test_*.png`.

- `astronaut_original_512.png`: fotografía de ejemplo incluida en `skimage.data.astronaut`.
- `astronaut_faceswap_512.png`: versión manipulada localmente sobre la región facial. La manipulación copia/refleja la cara, altera contraste/color y deja una discontinuidad suave en la zona del rostro. No pretende simular un deepfake perfecto, sino aportar una imagen fotográfica con una región manipulada clara para inspeccionar SRM, ELA y DCT.

Uso recomendado:

```bash
./build/detect_seq Task1/images/astronaut_faceswap_512.png --block-size 8
```

Salidas esperadas para comentar en la memoria:

- `srm_3x3.png`: resalta bordes finos, texturas y discontinuidades locales alrededor del rostro manipulado.
- `srm_5x5.png`: respuesta más amplia y suavizada; útil para ver cambios de textura menos puntuales.
- `ela.png`: muestra diferencias de recompresión JPEG; la región manipulada puede destacar si su historia de compresión no coincide con el fondo.
- `dct_invert.png`: reconstrucción de altas frecuencias tras anular bajas frecuencias por bloque.
- `dct_direct_local.png`: DCT directa normalizada por bloque; buena para visualizar cada bloque, pero no conserva energía relativa entre regiones.
- `dct_direct_global.png`: DCT directa normalizada una sola vez a escala global; es la salida correcta para comparar energía relativa entre zonas.
