#include "dct.h"
#include <algorithm>
#include <cmath>
#include <mutex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
constexpr int LUT_MAX = 65;
float g_cos_lut[LUT_MAX][LUT_MAX][LUT_MAX];
std::once_flag g_lut_once[LUT_MAX];

void ensure_lut(int size) {
    if (size <= 0 || size >= LUT_MAX) return;
    std::call_once(g_lut_once[size], [size] {
        for (int i = 0; i < size; ++i) {
            for (int k = 0; k < size; ++k) {
                g_cos_lut[size][i][k] = std::cos((2.0f * k + 1.0f) * i * static_cast<float>(M_PI) / (2.0f * size));
            }
        }
    });
}
}

void dct::direct(float** dct_matrix, const Block<float>& matrix, int channel) {
    const int size = matrix.size;
    ensure_lut(size);

    const float inv_sqrt_size = 1.0f / std::sqrt(static_cast<float>(size));
    const float sqrt2 = std::sqrt(2.0f);

    for (int i = 0; i < size; ++i) {
        const float ci = (i == 0) ? inv_sqrt_size : sqrt2 * inv_sqrt_size;
        for (int j = 0; j < size; ++j) {
            const float cj = (j == 0) ? inv_sqrt_size : sqrt2 * inv_sqrt_size;
            float sum = 0.0f;
            for (int k = 0; k < size; ++k) {
                const float ck = g_cos_lut[size][i][k];
                for (int l = 0; l < size; ++l) {
                    sum += matrix.get_pixel(k, l, channel) * ck * g_cos_lut[size][j][l];
                }
            }
            dct_matrix[i][j] = ci * cj * sum;
        }
    }
}

void dct::inverse(Block<float>& idct_matrix, float** dct_matrix, int channel, float min_value, float max_value) {
    const int size = idct_matrix.size;
    ensure_lut(size);

    const float inv_sqrt2 = 1.0f / std::sqrt(2.0f);
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            float sum = 0.0f;
            for (int u = 0; u < size; ++u) {
                const float cu_factor = (u == 0) ? inv_sqrt2 : 1.0f;
                const float cu = g_cos_lut[size][u][i];
                for (int v = 0; v < size; ++v) {
                    const float cv_factor = (v == 0) ? inv_sqrt2 : 1.0f;
                    sum += dct_matrix[u][v] * cu_factor * cv_factor * cu * g_cos_lut[size][v][j];
                }
            }
            const float value = std::clamp(0.25f * sum, min_value, max_value);
            idct_matrix.set_pixel(i, j, channel, value);
        }
    }
}

void dct::normalize(float** dct_matrix, int size) {
    float max_v = -1e30f;
    float min_v = 1e30f;
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            min_v = std::min(min_v, dct_matrix[i][j]);
            max_v = std::max(max_v, dct_matrix[i][j]);
        }
    }
    float range = max_v - min_v;
    if (range == 0.0f) range = 1.0f;
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            dct_matrix[i][j] = 255.0f * (dct_matrix[i][j] - min_v) / range;
        }
    }
}

void dct::assign(float** dct_matrix, Block<float>& block, int channel) {
    for (int i = 0; i < block.size; ++i) {
        for (int j = 0; j < block.size; ++j) {
            block.set_pixel(i, j, channel, dct_matrix[i][j]);
        }
    }
}

float** dct::create_matrix(int x_size, int y_size) {
    float** m = new float*[x_size];
    float* p = new float[x_size * y_size]{};
    for (int i = 0; i < x_size; ++i) {
        m[i] = &p[i * y_size];
    }
    return m;
}

void dct::delete_matrix(float** m) {
    delete[] m[0];
    delete[] m;
}
