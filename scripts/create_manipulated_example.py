#!/usr/bin/env python3
"""Regenera la imagen fotográfica manipulada usada en la defensa.

Requiere Pillow y scikit-image. La entrega ya incluye los PNGs generados, por lo
que este script es solo trazabilidad/reproducibilidad.
"""
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter
from skimage import data

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "Task1" / "images"
OUT.mkdir(parents=True, exist_ok=True)

img = Image.fromarray(data.astronaut()).convert("RGB").resize((512, 512))
img.save(OUT / "astronaut_original_512.png")

face_box = (168, 55, 294, 190)
face = img.crop(face_box)
patch = face.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
patch = ImageEnhance.Color(patch).enhance(0.75)
patch = ImageEnhance.Contrast(patch).enhance(1.18)
patch = patch.filter(ImageFilter.GaussianBlur(radius=0.4))

mask = Image.new("L", patch.size, 0)
draw = ImageDraw.Draw(mask)
draw.ellipse((8, 4, patch.size[0] - 8, patch.size[1] - 6), fill=230)
mask = mask.filter(ImageFilter.GaussianBlur(radius=2))

manip = img.copy()
manip.paste(patch, face_box[:2], mask)
arr = np.array(manip).astype(np.int16)
x1, y1, x2, y2 = 214, 126, 270, 168
arr[y1:y2, x1:x2, 0] = np.clip(arr[y1:y2, x1:x2, 0] + 16, 0, 255)
arr[y1:y2, x1:x2, 1] = np.clip(arr[y1:y2, x1:x2, 1] - 10, 0, 255)
manip = Image.fromarray(arr.astype(np.uint8), "RGB")
manip.save(OUT / "astronaut_faceswap_512.png")
manip.resize((1024, 1024), Image.Resampling.BICUBIC).save(OUT / "astronaut_faceswap_1024.png")
print(f"Imágenes guardadas en {OUT}")
