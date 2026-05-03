#ifndef __IMAGE__H__
#define __IMAGE__H__
#include <vector>
#include <memory>
#include <iostream>
#include "assert.h"
#include <string>
#ifdef _OPENMP
#include <omp.h>
#endif

// Umbral mínimo (en píxeles) por debajo del cual NO se paraleliza un bucle.
// Para imágenes pequeñas el overhead de crear el equipo de hilos OpenMP es
// mayor que el coste del bucle.  Empíricamente: 64x64 ya merece la pena
// con dos cores; debajo, no.  Se sobreescribe vía -DIMG_OMP_THRESHOLD=N.
#ifndef IMG_OMP_THRESHOLD
#define IMG_OMP_THRESHOLD 65536
#endif

template <typename T> class Block;

template <typename T> class Image{
public:
  int width, height, channels;
  std::shared_ptr<T[]> matrix;
  void release();
  Image();
  Image(int width, int height, int channels);
  Image(const Image<T> &a);
  Image(Image<T> &&a) noexcept = default;
  ~Image();
  Image<T>& operator=(const Image<T>& other);
  Image<T>& operator=(Image<T>&& other) noexcept = default;
  Image<T> operator*(const Image<T>& other) const;
  Image<T> operator*(float scalar) const;
  Image<T> operator+(const Image<T>& other) const;
  Image<T> operator+(float scalar) const;
  T get(int row, int col, int channel) const;
  void set(int row, int col, int channel, T value);
  template <typename S> Image<S> convert() const;
  Image<T> to_grayscale() const;
  Image<T> abs() const;
  Image<float> normalized() const;
  Image<T> convolution(const Image<float> &kernel) const;
  std::vector<Block<T>> get_blocks(int block_size=8);
};

Image<unsigned char> load_from_file(const std::string &filename);
void save_to_file(const std::string &filename, const Image<unsigned char> &image, int quality=100);

template <typename T> class Block{
public:
    int i, j, size, depth, rowsize;
    Image<T> *matrix;
    T get_pixel(int row, int col, int channel) const;
    void set_pixel(int row, int col, int channel, T value);
};

template <class T> T Block<T>::get_pixel(int row, int col, int channel) const {
    assert(row>=0 && row<size && col>=0 && col<size);
    return matrix->get(row+j, col+i, channel);
}
template <class T> void Block<T>::set_pixel(int row, int col, int channel, T value) {
    assert(row>=0 && row<size && col>=0 && col<size);
    return matrix->set(row+j, col+i, channel, value);
}

template <class T> Image<T>::Image() {
    width = 0;
    height = 0;
    channels = 0;
    matrix = nullptr;
}
template <class T> Image<T>::Image(int width, int height, int channels) {
    this->width = width;
    this->height = height;
    this->channels = channels;
    matrix = std::shared_ptr<T[]>(new T[height*width*channels]);
}
template <class T> Image<T>::Image(const Image<T> &a) {
    width = a.width;
    height = a.height;
    channels = a.channels;
    if (a.matrix != nullptr) matrix = a.matrix;
    else                  matrix = nullptr;
}
template <class T> Image<T>::~Image() { release(); }

template <class T> Image<T>& Image<T>::operator=(const Image<T> &a) {
    if (this == &a) return *this;
    release();
    width = a.width;
    height = a.height;
    channels = a.channels;
    if (a.matrix != nullptr) matrix = a.matrix;
    else                  matrix = nullptr;
    return *this;
}

template <class T> void Image<T>::release() { matrix = nullptr; }

template <class T> T Image<T>::get(int row, int col, int channel) const{
    return matrix[row*width*channels + col*channels + channel];
}
template <class T> void Image<T>::set(int row, int col, int channel, T value) {
    matrix[row*width*channels + col*channels + channel] = value;
}

// ---------------------------------------------------------------------------
// PARALELIZACIÓN OPENMP — Operadores aritméticos píxel-a-píxel.
// El bucle externo (filas, j) es independiente entre iteraciones (cada hilo
// escribe a una banda distinta de la imagen). Usamos `collapse(2)` para que
// el reparto de trabajo cubra también el bucle interno cuando el ancho es
// grande, mejorando el balanceo. `schedule(static)` es óptimo porque cada
// iteración tiene coste constante.
// ---------------------------------------------------------------------------
template <class T> Image<T> Image<T>::operator*(const Image<T>& other) const {
    assert(width == other.width && height == other.height && channels == other.channels);
    Image<T> new_image(width, height, channels);
    #pragma omp parallel for collapse(2) schedule(static) if(width*height > IMG_OMP_THRESHOLD && omp_get_max_threads() > 1)
    for(int j=0;j<height;j++)
        for(int i=0;i<width;i++)
            for(int c=0;c<channels;c++)
                new_image.set(j, i, c, this->get(j, i, c) * other.get(j, i, c));
    return new_image;
}
template <class T> Image<T> Image<T>::operator*(float scalar) const {
    Image<T> new_image(width, height, channels);
    #pragma omp parallel for collapse(2) schedule(static) if(width*height > IMG_OMP_THRESHOLD && omp_get_max_threads() > 1)
    for(int j=0;j<height;j++)
        for(int i=0;i<width;i++)
            for(int c=0;c<channels;c++)
                new_image.set(j, i, c, (T)(this->get(j, i, c)*scalar));
    return new_image;
}
template <class T> Image<T> Image<T>::operator+(const Image<T>& other) const {
    assert(width == other.width && height == other.height && channels == other.channels);
    Image<T> new_image(width, height, channels);
    #pragma omp parallel for collapse(2) schedule(static) if(width*height > IMG_OMP_THRESHOLD && omp_get_max_threads() > 1)
    for(int j=0;j<height;j++)
        for(int i=0;i<width;i++)
            for(int c=0;c<channels;c++)
                new_image.set(j,i,c, this->get(j, i,c)+other.get(j, i, c));
    return new_image;
}
template <class T> Image<T> Image<T>::operator+(float scalar) const {
    Image<T> new_image(width, height, channels);
    #pragma omp parallel for collapse(2) schedule(static) if(width*height > IMG_OMP_THRESHOLD && omp_get_max_threads() > 1)
    for(int j=0;j<height;j++)
        for(int i=0;i<width;i++)
            for(int c=0;c<channels;c++)
                new_image.set(j, i, c, ((T)this->get(j, i, c)+scalar));
    return new_image;
}
template <class T> Image<T> Image<T>::abs() const {
    Image<T> new_image(width, height, channels);
    #pragma omp parallel for collapse(2) schedule(static) if(width*height > IMG_OMP_THRESHOLD && omp_get_max_threads() > 1)
    for(int j=0;j<height;j++)
        for(int i=0;i<width;i++)
            for(int c=0;c<channels;c++)
                new_image.set(j, i, c, (T)std::abs(this->get(j,i,c)));
    return new_image;
}

// ---------------------------------------------------------------------------
// PARALELIZACIÓN OPENMP — Convolución 2D.
// Es el bucle más costoso de SRM: O(W·H·C·K²). El bucle exterior por filas
// es totalmente independiente — cada hilo computa filas distintas del
// resultado y sólo lee de `*this`. Las variables `u,v,s,t,sum` deben ser
// PRIVADAS por iteración (se declaran dentro). Usamos `collapse(2)` y
// `schedule(static)` (carga uniforme).
// ---------------------------------------------------------------------------
template <class T> Image<T> Image<T>::convolution(const Image<float> &kernel) const {
    assert(kernel.width%2 != 0 && kernel.height%2 != 0 && kernel.width == kernel.height && kernel.channels==1);
    int kernel_size = kernel.width;
    Image<T> convolved(width, height, channels);
    #pragma omp parallel for collapse(2) schedule(static) if(width*height > IMG_OMP_THRESHOLD && omp_get_max_threads() > 1)
    for(int j=0;j<height;j++){
        for(int i=0;i<width; i++){
            for(int c=0;c<channels;c++){
                float sum = 0.0;
                for(int u=0;u<kernel_size;u++){
                    for(int v=0;v<kernel_size;v++){
                        int s = (j + u - kernel_size/2)%height;
                        int t = (i + v - kernel_size/2)%width;
                        if (s < 0 || s >= height || t < 0 || t >= width)
                            continue;
                        sum += (this->get(s, t, c) * kernel.get(u,v, 0));
                    }
                }
                convolved.set(j, i, c, (T)sum/(kernel_size*kernel_size));
            }
        }
    }
    return convolved;
}

template <class T> template <typename S> Image<S> Image<T>::convert() const {
    Image<S> new_image(width, height, channels);
    #pragma omp parallel for collapse(2) schedule(static) if(width*height > IMG_OMP_THRESHOLD && omp_get_max_threads() > 1)
    for(int j=0;j<height;j++)
        for(int i=0;i<width;i++)
            for(int c=0;c<channels;c++)
                new_image.set(j, i, c, (S)this->get(j, i, c));
    return new_image;
}

template <class T> Image<T> Image<T>::to_grayscale() const {
    if (channels == 1) return convert<T>();
    Image<T> image(width, height, 1);
    #pragma omp parallel for collapse(2) schedule(static) if(width*height > IMG_OMP_THRESHOLD && omp_get_max_threads() > 1)
    for(int j=0;j<height;j++)
        for(int i=0;i<width;i++)
            image.set(j, i, 0, (T)((0.299 * this->get(j, i, 0) + (0.587 * this->get(j, i, 1)) + (0.114 * this->get(j,i,2)))));
    return image;
}

// ---------------------------------------------------------------------------
// PARALELIZACIÓN OPENMP — `normalized()` con reducción.
// Necesita primero computar min/max sobre toda la imagen (REDUCCIÓN) y
// después aplicar la fórmula píxel a píxel. Las clausulas `reduction(min:)`
// y `reduction(max:)` (OpenMP 3.1+) son la forma idiomática de evitar carreras.
// ---------------------------------------------------------------------------
template <class T> Image<float> Image<T>::normalized() const {
    Image<float> new_image(width, height, channels);
    float max_value = -1e30f;
    float min_value =  1e30f;

    #pragma omp parallel for collapse(2) schedule(static) reduction(max:max_value) reduction(min:min_value) if(width*height > IMG_OMP_THRESHOLD && omp_get_max_threads() > 1)
    for(int j=0;j<height;j++)
        for(int i=0;i<width;i++)
            for(int c=0;c<channels;c++){
                float v = (float)this->get(j,i,c);
                if (v > max_value) max_value = v;
                if (v < min_value) min_value = v;
            }

    float range = (max_value - min_value);
    if (range == 0.0f) range = 1.0f;
    #pragma omp parallel for collapse(2) schedule(static) if(width*height > IMG_OMP_THRESHOLD && omp_get_max_threads() > 1)
    for(int j=0;j<height;j++)
        for(int i=0;i<width;i++)
            for(int c=0;c<channels;c++)
                new_image.set(j,i,c, ((float)this->get(j, i, c)-min_value) / range);
    return new_image;
}

template <class T> std::vector<Block<T>> Image<T>::get_blocks(int block_size) {
    int depth = channels;
    assert(width % block_size == 0 && height % block_size == 0);
    std::vector<Block<T>> blocks;
    // Reservamos para evitar realloc dentro del bucle (mejor para paralelizar)
    blocks.reserve((height/block_size) * (width/block_size));
    for (int row=0;row<height;row+=block_size)
        for(int col=0;col<width;col+=block_size){
            Block<T> b;
            b.i=col;
            b.j=row;
            b.size=block_size;
            b.rowsize=width*channels;
            b.matrix=this;
            b.depth=depth;
            blocks.push_back(b);
        }
    return blocks;
}

#endif
