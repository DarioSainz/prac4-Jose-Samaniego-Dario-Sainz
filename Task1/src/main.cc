#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#ifndef DISABLE_ASYNC
#include <future>
#endif
#include <iostream>
#include <sstream>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "utils/dct.h"
#include "utils/image.h"

#ifndef DCT_OMP_BLOCK_THRESHOLD
#define DCT_OMP_BLOCK_THRESHOLD 128
#endif

namespace {
constexpr int kElaQuality = 90;
constexpr int kPipelineTasks = 6;
std::mutex g_log_mutex;

enum class DctMode {
    InverseHighFreq,
    DirectLocal,
    DirectGlobal,
};

void print_usage(const char* argv0) {
    std::cerr
        << "Usage: " << argv0 << " <imagen.png|imagen.jpg> [num_threads] "
        << "[--block-size 8|16|32] [--no-async] [--no-output] [--csv <file>]\n"
        << "\n"
        << "  num_threads       Presupuesto OpenMP. En detect híbrido se reparte entre tareas async.\n"
        << "  --block-size N    Tamaño de bloque DCT. Valores permitidos: 8, 16 o 32. Default: 8.\n"
        << "  --no-output       Ejecuta el cómputo pero no escribe PNGs finales; útil para benchmarks.\n"
        << "  --csv <file>      Añade una fila con tiempos por etapa.\n";
}

bool valid_block_size(int block_size) {
    return block_size == 8 || block_size == 16 || block_size == 32;
}

void log_elapsed(const std::string& label, long long ms) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::cout << label << " elapsed: " << ms << "ms" << std::endl;
}

struct ProcResult {
    Image<unsigned char> image;
    long long ms = 0;
};

std::string unique_tmp_jpeg() {
    static std::atomic<unsigned long> counter{0};
    std::ostringstream oss;
    oss << "ela_" << static_cast<long>(getpid()) << "_"
        << std::this_thread::get_id() << "_"
        << counter.fetch_add(1, std::memory_order_relaxed) << ".jpg";
    return (std::filesystem::temp_directory_path() / oss.str()).string();
}

Image<float> get_srm_3x3() {
    Image<float> kernel(3, 3, 1);
    kernel.set(0, 0, 0, -1); kernel.set(0, 1, 0, 2);  kernel.set(0, 2, 0, -1);
    kernel.set(1, 0, 0, 2);  kernel.set(1, 1, 0, -4); kernel.set(1, 2, 0, 2);
    kernel.set(2, 0, 0, -1); kernel.set(2, 1, 0, 2);  kernel.set(2, 2, 0, -1);
    return kernel;
}

Image<float> get_srm_5x5() {
    Image<float> kernel(5, 5, 1);
    kernel.set(0, 0, 0, -1); kernel.set(0, 1, 0, 2);  kernel.set(0, 2, 0, -2);  kernel.set(0, 3, 0, 2);  kernel.set(0, 4, 0, -1);
    kernel.set(1, 0, 0, 2);  kernel.set(1, 1, 0, -6); kernel.set(1, 2, 0, 8);   kernel.set(1, 3, 0, -6); kernel.set(1, 4, 0, 2);
    kernel.set(2, 0, 0, -2); kernel.set(2, 1, 0, 8);  kernel.set(2, 2, 0, -12); kernel.set(2, 3, 0, 8);  kernel.set(2, 4, 0, -2);
    kernel.set(3, 0, 0, 2);  kernel.set(3, 1, 0, -6); kernel.set(3, 2, 0, 8);   kernel.set(3, 3, 0, -6); kernel.set(3, 4, 0, 2);
    kernel.set(4, 0, 0, -1); kernel.set(4, 1, 0, 2);  kernel.set(4, 2, 0, -2);  kernel.set(4, 3, 0, 2);  kernel.set(4, 4, 0, -1);
    return kernel;
}

Image<float> get_srm_kernel(int size) {
    if (size == 3) return get_srm_3x3();
    if (size == 5) return get_srm_5x5();
    std::cerr << "ERROR: kernel SRM no soportado: " << size << std::endl;
    std::exit(3);
}

ProcResult compute_srm(const Image<unsigned char>& image, int kernel_size) {
    const auto begin = std::chrono::steady_clock::now();
    Image<float> srm = image.to_grayscale().convert<float>();
    srm = srm.convolution(get_srm_kernel(kernel_size));
    srm = srm.abs().normalized() * 255.0f;
    Image<unsigned char> result = srm.convert<unsigned char>();
    const auto end = std::chrono::steady_clock::now();
    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    log_elapsed("SRM " + std::to_string(kernel_size) + "x" + std::to_string(kernel_size), ms);
    return {result, ms};
}

ProcResult compute_dct(const Image<unsigned char>& image, int block_size, DctMode mode) {
    const auto begin = std::chrono::steady_clock::now();
    Image<float> grayscale = image.convert<float>().to_grayscale();
    std::vector<Block<float>> blocks = grayscale.get_blocks(block_size);
    const int nb = static_cast<int>(blocks.size());
    const bool invert = (mode == DctMode::InverseHighFreq);
    const bool local_normalize = (mode == DctMode::DirectLocal);

#ifdef _OPENMP
    if (nb >= DCT_OMP_BLOCK_THRESHOLD && omp_get_max_threads() > 1) {
        #pragma omp parallel
        {
            float** dct_block = dct::create_matrix(block_size, block_size);
            #pragma omp for schedule(static)
            for (int i = 0; i < nb; ++i) {
                dct::direct(dct_block, blocks[i], 0);
                if (invert) {
                    // Elimina bajas frecuencias y reconstruye la imagen para resaltar textura/alta frecuencia.
                    for (int row = 0; row < blocks[i].size / 2; ++row) {
                        for (int col = 0; col < blocks[i].size / 2; ++col) {
                            dct_block[row][col] = 0.0f;
                        }
                    }
                    dct::inverse(blocks[i], dct_block, 0, 0.0f, 255.0f);
                } else {
                    if (local_normalize) {
                        // Visualización local: cada bloque 8/16/32 se escala por separado.
                        // Útil para inspección visual, pero NO conserva energía relativa entre regiones.
                        dct::normalize(dct_block, block_size);
                    }
                    dct::assign(dct_block, blocks[i], 0);
                }
            }
            dct::delete_matrix(dct_block);
        }
    } else
#endif
    {
        float** dct_block = dct::create_matrix(block_size, block_size);
        for (int i = 0; i < nb; ++i) {
            dct::direct(dct_block, blocks[i], 0);
            if (invert) {
                for (int row = 0; row < blocks[i].size / 2; ++row) {
                    for (int col = 0; col < blocks[i].size / 2; ++col) {
                        dct_block[row][col] = 0.0f;
                    }
                }
                dct::inverse(blocks[i], dct_block, 0, 0.0f, 255.0f);
            } else {
                if (local_normalize) {
                    dct::normalize(dct_block, block_size);
                }
                dct::assign(dct_block, blocks[i], 0);
            }
        }
        dct::delete_matrix(dct_block);
    }

    if (mode == DctMode::DirectGlobal) {
        // Visualización global: se normaliza una sola vez tras juntar todos los bloques.
        // Esta salida conserva la energía relativa entre regiones y es la adecuada para comparar zonas.
        grayscale = grayscale.normalized() * 255.0f;
    }

    Image<unsigned char> result = grayscale.convert<unsigned char>();
    const auto end = std::chrono::steady_clock::now();
    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    const char* label = (mode == DctMode::InverseHighFreq) ? "Inverse DCT" :
                        (mode == DctMode::DirectLocal) ? "Direct DCT local" : "Direct DCT global";
    log_elapsed(label, ms);
    return {result, ms};
}

ProcResult compute_ela(const Image<unsigned char>& image, int quality) {
    const auto begin = std::chrono::steady_clock::now();
    const std::string tmp = unique_tmp_jpeg();
    Image<unsigned char> grayscale = image.to_grayscale();
    save_to_file(tmp, grayscale, quality);
    Image<float> compressed = load_from_file(tmp).convert<float>();
    std::remove(tmp.c_str());
    compressed = (compressed + (grayscale.convert<float>() * -1.0f)).abs().normalized() * 255.0f;
    Image<unsigned char> result = compressed.convert<unsigned char>();
    const auto end = std::chrono::steady_clock::now();
    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    log_elapsed("ELA", ms);
    return {result, ms};
}

void write_outputs(const ProcResult& srm3, const ProcResult& srm5, const ProcResult& ela,
                   const ProcResult& dct_inv, const ProcResult& dct_local, const ProcResult& dct_global) {
    save_to_file("srm_3x3.png", srm3.image);
    save_to_file("srm_5x5.png", srm5.image);
    save_to_file("ela.png", ela.image);
    save_to_file("dct_invert.png", dct_inv.image);
    save_to_file("dct_direct_local.png", dct_local.image);
    save_to_file("dct_direct_global.png", dct_global.image);
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

#ifndef DISABLE_ASYNC
    bool use_async = true;
#else
    constexpr bool use_async = false;
#endif

    int requested_threads = -1;
    int block_size = 8;
    bool write_images = true;
    std::string csv_path;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--no-async") == 0) {
#ifndef DISABLE_ASYNC
            use_async = false;
#endif
        } else if (std::strcmp(argv[i], "--no-output") == 0) {
            write_images = false;
        } else if (std::strcmp(argv[i], "--block-size") == 0 && i + 1 < argc) {
            block_size = std::atoi(argv[++i]);
        } else if (std::strncmp(argv[i], "--block-size=", 13) == 0) {
            block_size = std::atoi(argv[i] + 13);
        } else if (std::strcmp(argv[i], "--csv") == 0 && i + 1 < argc) {
            csv_path = argv[++i];
        } else if (argv[i][0] != '-') {
            requested_threads = std::atoi(argv[i]);
        } else {
            std::cerr << "ERROR: argumento no reconocido: " << argv[i] << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!valid_block_size(block_size)) {
        std::cerr << "ERROR: --block-size solo admite 8, 16 o 32" << std::endl;
        return 5;
    }

#ifdef _OPENMP
    if (requested_threads > 0) omp_set_num_threads(requested_threads);
    omp_set_dynamic(0);
    // omp_set_nested está deprecated desde OpenMP 5.0. La forma moderna
    // de desactivar el paralelismo anidado es fijar el número máximo de
    // niveles activos a 1: el equipo OpenMP no se anidará dentro de cada
    // tarea std::async.
    omp_set_max_active_levels(1);
    int csv_threads = omp_get_max_threads();
#ifndef DISABLE_ASYNC
    if (use_async) {
        const int budget = omp_get_max_threads();
        const int per_task = std::max(1, budget / kPipelineTasks);
        // En la variante híbrida, num_threads actúa como presupuesto para los
        // equipos OpenMP internos. Además, std::async lanza tareas funcionales
        // independientes. Se reparte el presupuesto OpenMP para reducir la
        // sobre-suscripción.
        omp_set_num_threads(per_task);
        std::cout << "OpenMP threads (per async task) = " << per_task
                  << " [budget total = " << budget << "]" << std::endl;
    } else {
        std::cout << "OpenMP threads = " << omp_get_max_threads() << std::endl;
    }
#else
    std::cout << "OpenMP threads = " << omp_get_max_threads() << std::endl;
#endif
#else
    (void)requested_threads;
    const int csv_threads = 1;
    std::cout << "OpenMP DISABLED at compile time" << std::endl;
#endif

    std::cout << "async paralelismo funcional = " << (use_async ? "ON" : "OFF") << std::endl;
    std::cout << "DCT block_size = " << block_size << std::endl;

    Image<unsigned char> image = load_from_file(argv[1]);
    if (image.matrix == nullptr) {
        std::cerr << "ERROR: no he podido cargar " << argv[1] << std::endl;
        return 2;
    }
    if (image.width != image.height || image.width % block_size != 0) {
        std::cerr << "ERROR: la imagen debe ser cuadrada y múltiplo de " << block_size << std::endl;
        return 4;
    }
    std::cout << "Image: " << image.width << "x" << image.height
              << " channels=" << image.channels << std::endl;

    const auto total_begin = std::chrono::steady_clock::now();
    ProcResult r_srm3, r_srm5, r_ela, r_dct_inv, r_dct_local, r_dct_global;

#ifndef DISABLE_ASYNC
    if (use_async) {
        auto f_srm3     = std::async(std::launch::async, [&image] { return compute_srm(image, 3); });
        auto f_srm5     = std::async(std::launch::async, [&image] { return compute_srm(image, 5); });
        auto f_ela      = std::async(std::launch::async, [&image] { return compute_ela(image, kElaQuality); });
        auto f_dctinv   = std::async(std::launch::async, [&image, block_size] { return compute_dct(image, block_size, DctMode::InverseHighFreq); });
        auto f_dctlocal = std::async(std::launch::async, [&image, block_size] { return compute_dct(image, block_size, DctMode::DirectLocal); });
        auto f_dctglob  = std::async(std::launch::async, [&image, block_size] { return compute_dct(image, block_size, DctMode::DirectGlobal); });

        r_srm3       = f_srm3.get();
        r_srm5       = f_srm5.get();
        r_ela        = f_ela.get();
        r_dct_inv    = f_dctinv.get();
        r_dct_local  = f_dctlocal.get();
        r_dct_global = f_dctglob.get();
    } else
#endif
    {
        r_srm3       = compute_srm(image, 3);
        r_srm5       = compute_srm(image, 5);
        r_ela        = compute_ela(image, kElaQuality);
        r_dct_inv    = compute_dct(image, block_size, DctMode::InverseHighFreq);
        r_dct_local  = compute_dct(image, block_size, DctMode::DirectLocal);
        r_dct_global = compute_dct(image, block_size, DctMode::DirectGlobal);
    }

    const auto total_end = std::chrono::steady_clock::now();
    const long long total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_begin).count();
    std::cout << "TOTAL pipeline: " << total_ms << "ms" << std::endl;

    if (write_images) {
        write_outputs(r_srm3, r_srm5, r_ela, r_dct_inv, r_dct_local, r_dct_global);
    }

    if (!csv_path.empty()) {
        FILE* f = std::fopen(csv_path.c_str(), "r");
        const bool exists = (f != nullptr);
        if (f) std::fclose(f);
        f = std::fopen(csv_path.c_str(), "a");
        if (f) {
            if (!exists) {
                std::fprintf(f, "size,threads,async,block_size,total_ms,srm3_ms,srm5_ms,ela_ms,dct_inv_ms,dct_local_ms,dct_global_ms\n");
            }
            std::fprintf(f, "%d,%d,%d,%d,%lld,%lld,%lld,%lld,%lld,%lld,%lld\n",
                         image.width, csv_threads, use_async ? 1 : 0, block_size, total_ms,
                         r_srm3.ms, r_srm5.ms, r_ela.ms, r_dct_inv.ms, r_dct_local.ms, r_dct_global.ms);
            std::fclose(f);
        }
    }

    return 0;
}
