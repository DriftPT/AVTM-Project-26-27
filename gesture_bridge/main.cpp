#include <iostream>
#include <cmath>
#include <algorithm>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <k4a/k4a.h>
#include <k4abt.h>

// ---- PLACAS (vermelha = PAUSE, azul claro = RESUME) ----
const int   STOP_HOLD_FRAMES    = 5;       // frames com a placa vermelha para pausar (~0,16 s a 30 fps)
const int   RESUME_HOLD_FRAMES  = 5;       // frames com a placa azul para retomar

const float SIGN_FRONT_MM       = 150.0f;  // pixel tem de estar pelo menos isto mais perto da câmara que o ombro
const float SIGN_BOX_HALF_W_MM  = 450.0f;  // zona à volta do ombro (largura/2)
const float SIGN_BOX_HALF_H_MM  = 350.0f;  // zona à volta do ombro (altura/2)
const float SIGN_MIN_AREA_CM2   = 60.0f;   // área mínima de cor para contar como placa
const bool  DEBUG_SIGN          = false;    // imprime as áreas de vermelho e azul (quando > 5 cm2)
const int   COLOR_EXPOSURE_US   = 16670;   // exposição fixa (µs). 2500 deixava as placas quase pretas; sobe/desce conforme a corMedia
const bool  DEBUG_STATUS        = false;    // imprime SEMPRE (3x/s) o estado de cada passo da deteção

// ---- CHAMA (vela / isqueiro) ----
const int FLAME_MIN_PIXELS  = 8;   // para acender
const int FLAME_KEEP_PIXELS = 3;   // para manter acesa
const int FLAME_ON_FRAMES   = 3;   // frames seguidos acima de MIN para ligar (anti-ruído)
const int FLAME_OFF_FRAMES  = 10;  // frames seguidos abaixo de KEEP para desligar (~0,3 s)

struct UdpSender {
    SOCKET s = INVALID_SOCKET;
    sockaddr_in addr{};

    bool init(const char* ip, unsigned short port) {
        // inicializa Winsock (Como é windows, é necessário)
        WSADATA w;
        if (WSAStartup(MAKEWORD(2, 2), &w) != 0) return false;

        s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // cria o socket UDP
        if (s == INVALID_SOCKET) return false;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);  // converte o port para network byte order
        inet_pton(AF_INET, ip, &addr.sin_addr);
        return true;
    }
    void send(const char* msg) {
        if (s != INVALID_SOCKET)
            sendto(s, msg, (int)strlen(msg), 0, (sockaddr*)&addr, sizeof(addr));
    }
    ~UdpSender() {
        if (s != INVALID_SOCKET) closesocket(s);
        WSACleanup();
    }
};
UdpSender g_udp;

// ===================== PLACAS =====================

// Vermelho dominante (relativo, funciona com exposição baixa).
static inline bool isRedPixel(const uint8_t* p) {
    int b = p[0], g = p[1], r = p[2];
    return r >= 40 && r >= 2 * g && r >= 2 * b;
}

// Azul claro: B é o canal mais forte, G pode ser alto (ciano/azul céu), R é o mais baixo.
// A pele (R > B) e a chama (R alto) não passam.
static inline bool isBluePixel(const uint8_t* p) {
    int b = p[0], g = p[1], r = p[2];
    return b >= 40 && b >= r + 20 && b >= g;
}

struct SignAreas {
    float red = 0.0f;
    float blue = 0.0f;
    // --- só para debug ---
    float pxPerMm = 0.0f;                 // 0 = a projeção do ombro para a imagem falhou
    int   frontPx = 0;                    // pixels com profundidade válida e à frente do ombro
    float avgR = 0, avgG = 0, avgB = 0;   // cor média desses pixels
};

// Áreas (cm2) de vermelho e de azul que estão MAIS PERTO da câmara do que o ombro.
// depthInColor = mapa de profundidade alinhado com a imagem de cor.
static SignAreas signAreasInFrontOfShoulder(k4a_image_t color, k4a_image_t depthInColor,
                                            const k4a_calibration_t& cal, const k4a_float3_t& shoulder) {
    SignAreas out;

    k4a_float3_t ref = shoulder;
    ref.xyz.x += 100.0f;   // ponto 100 mm ao lado, para medir pixels por mm

    k4a_float2_t c{}, r{};
    int v1 = 0, v2 = 0;
    if (K4A_RESULT_SUCCEEDED != k4a_calibration_3d_to_2d(&cal, &shoulder, K4A_CALIBRATION_TYPE_DEPTH,
                                                         K4A_CALIBRATION_TYPE_COLOR, &c, &v1) || !v1) return out;
    if (K4A_RESULT_SUCCEEDED != k4a_calibration_3d_to_2d(&cal, &ref, K4A_CALIBRATION_TYPE_DEPTH,
                                                         K4A_CALIBRATION_TYPE_COLOR, &r, &v2) || !v2) return out;

    float pxPerMm = hypotf(r.xy.x - c.xy.x, r.xy.y - c.xy.y) / 100.0f;
    if (pxPerMm <= 0.0f) return out;

    int halfW = (int)(SIGN_BOX_HALF_W_MM * pxPerMm);
    int halfH = (int)(SIGN_BOX_HALF_H_MM * pxPerMm);

    const uint8_t* cbuf = k4a_image_get_buffer(color);
    int w = k4a_image_get_width_pixels(color);
    int h = k4a_image_get_height_pixels(color);
    int cstride = k4a_image_get_stride_bytes(color);

    const uint8_t* dbuf = k4a_image_get_buffer(depthInColor);
    int dstride = k4a_image_get_stride_bytes(depthInColor);

    int x0 = (std::max)(0, (int)c.xy.x - halfW), x1 = (std::min)(w, (int)c.xy.x + halfW);
    int y0 = (std::max)(0, (int)c.xy.y - halfH), y1 = (std::min)(h, (int)c.xy.y + halfH);
    if (x1 <= x0 || y1 <= y0) return out;

    float maxDepth = shoulder.xyz.z - SIGN_FRONT_MM;   // só pixels mais perto que isto
    int redCount = 0, blueCount = 0, frontCount = 0;
    long sumR = 0, sumG = 0, sumB = 0;
    for (int y = y0; y < y1; ++y) {
        const uint8_t* crow = cbuf + (size_t)y * cstride;
        const uint16_t* drow = (const uint16_t*)(dbuf + (size_t)y * dstride);
        for (int x = x0; x < x1; ++x) {
            uint16_t d = drow[x];
            if (d == 0 || d > maxDepth) continue;       // sem profundidade ou ao nível do corpo
            const uint8_t* p = crow + x * 4;
            ++frontCount;
            sumB += p[0]; sumG += p[1]; sumR += p[2];
            if (isRedPixel(p))       ++redCount;
            else if (isBluePixel(p)) ++blueCount;
        }
    }
    out.pxPerMm = pxPerMm;
    out.frontPx = frontCount;
    if (frontCount > 0) {
        out.avgR = (float)sumR / frontCount;
        out.avgG = (float)sumG / frontCount;
        out.avgB = (float)sumB / frontCount;
    }

    // pixels -> cm2 (1 pixel = 1/pxPerMm^2 mm2 ; 100 mm2 = 1 cm2)
    float pxToCm2 = 1.0f / (pxPerMm * pxPerMm) / 100.0f;
    out.red  = redCount  * pxToCm2;
    out.blue = blueCount * pxToCm2;
    return out;
}

// Máquina de estados simples:
//  - a jogar  -> placa vermelha envia PAUSE
//  - em pausa -> placa azul envia RESUME
// Cada placa só conta no estado em que faz sentido, por isso manter a placa levantada não repete o comando.
struct GestureTracker {
    bool paused = false;
    int holdFrames = 0;

    void process(const SignAreas& a) {
        if (DEBUG_SIGN && (a.red > 5.0f || a.blue > 5.0f))
            printf("[DEBUG] vermelho = %.0f cm2   azul = %.0f cm2   %s\n",
                   a.red, a.blue, paused ? "[PAUSADO]" : "");

        float area = paused ? a.blue : a.red;
        int needed = paused ? RESUME_HOLD_FRAMES : STOP_HOLD_FRAMES;

        if (area >= SIGN_MIN_AREA_CM2) {
            if (++holdFrames >= needed) {
                if (paused) {
                    std::cout << "\n>>> [GESTURE] RESUME (placa azul) <<<\n";
                    g_udp.send("RESUME");
                } else {
                    std::cout << "\n>>> [GESTURE] STOP (placa vermelha) <<<\n";
                    g_udp.send("PAUSE");
                }
                paused = !paused;
                holdFrames = 0;
            }
        } else if (holdFrames > 0) {
            --holdFrames;   // perdoa frames maus isolados
        }
    }
};

// ===================== CHAMA =====================
// Critério: Vermelho forte, Verde médio/alto, Azul fraco
static inline bool isFlamePixel(const uint8_t* p) {
    int b = p[0], g = p[1], r = p[2];
    return r >= 250 && g >= 170 && r > g && g > b + 40;
}

static int countFlamePixels(k4a_image_t color) {
    const uint8_t* buf = k4a_image_get_buffer(color);
    int w = k4a_image_get_width_pixels(color);
    int h = k4a_image_get_height_pixels(color);
    int stride = k4a_image_get_stride_bytes(color);

    int count = 0;
    for (int y = 0; y < h; ++y) {
        const uint8_t* row = buf + (size_t)y * stride;
        for (int x = 0; x < w; ++x)
            if (isFlamePixel(row + x * 4)) ++count;
    }
    return count;
}

struct FlameTracker {
    bool lit = false;
    int onFrames = 0;
    int offFrames = 0;

    void process(int pixels) {
        if (!lit) {
            if (pixels >= FLAME_MIN_PIXELS) {
                if (++onFrames >= FLAME_ON_FRAMES) {
                    std::cout << "\n>>> [FLAME] LIT -> velas ON (" << pixels << " px) <<<\n";
                    g_udp.send("CANDLES_ON");
                    lit = true;
                    onFrames = 0;
                    offFrames = 0;
                }
            } else {
                onFrames = 0;
            }
        } else {
            if (pixels >= FLAME_KEEP_PIXELS) {
                offFrames = 0;
            } else if (++offFrames >= FLAME_OFF_FRAMES) {
                std::cout << "\n>>> [FLAME] OUT -> velas OFF <<<\n";
                g_udp.send("CANDLES_OFF");
                lit = false;
                onFrames = 0;
                offFrames = 0;
            }
        }
    }
};

int main() {
    if (!g_udp.init("127.0.0.1", 5005))
        std::cerr << "Falha a iniciar UDP (os gestos so vao aparecer na consola).\n";

    k4a_device_t device = NULL;
    if (K4A_RESULT_SUCCEEDED != k4a_device_open(0, &device)) {
        std::cerr << "Failed to open Femto Bolt camera.\n";
        return -1;
    }

    k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
    config.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
    config.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;     // necessário para a chama
    config.color_resolution = K4A_COLOR_RESOLUTION_720P;     // ligar a câmara de cor
    config.camera_fps = K4A_FRAMES_PER_SECOND_30;
    config.synchronized_images_only = false;                 // não perder frames de depth se a cor atrasar

    if (K4A_RESULT_SUCCEEDED != k4a_device_start_cameras(device, &config)) {
        std::cerr << "Failed to start camera streams.\n";
        k4a_device_close(device);
        return -1;
    }
    // Exposição fixa e baixa: só fontes muito luminosas (a chama) saturam
    k4a_device_set_color_control(device, K4A_COLOR_CONTROL_EXPOSURE_TIME_ABSOLUTE,
                                 K4A_COLOR_CONTROL_MODE_MANUAL, COLOR_EXPOSURE_US);
    k4a_device_set_color_control(device, K4A_COLOR_CONTROL_WHITEBALANCE,
                                 K4A_COLOR_CONTROL_MODE_MANUAL, 4500);

    k4a_calibration_t calibration;
    k4a_device_get_calibration(device, config.depth_mode, config.color_resolution, &calibration);

    k4abt_tracker_t tracker = NULL;
    k4abt_tracker_configuration_t tracker_config = K4ABT_TRACKER_CONFIG_DEFAULT;
    tracker_config.processing_mode = K4ABT_TRACKER_PROCESSING_MODE_GPU; 
    
    if (K4A_RESULT_SUCCEEDED != k4abt_tracker_create(&calibration, tracker_config, &tracker)) {
        std::cerr << "Failed to create body tracker.\n";
        k4a_device_stop_cameras(device);
        k4a_device_close(device);
        return -1;
    }

    // Transformação depth -> câmara de cor (para saber a profundidade de cada pixel de cor)
    k4a_transformation_t transformation = k4a_transformation_create(&calibration);
    k4a_image_t depthInColor = NULL;
    k4a_image_create(K4A_IMAGE_FORMAT_DEPTH16, 1280, 720, 1280 * (int)sizeof(uint16_t), &depthInColor);

    GestureTracker gesture_engine;
    FlameTracker flame_engine;
    std::cout << "Tracker running. Placa vermelha = PAUSE, placa azul = RESUME; acende uma vela/isqueiro para ligar as velas.\n";

    while (true) {
        k4a_capture_t capture = NULL;
        k4a_wait_result_t get_capture_result = k4a_device_get_capture(device, &capture, 100);

        if (get_capture_result == K4A_WAIT_RESULT_SUCCEEDED) {

            k4a_image_t color = k4a_capture_get_color_image(capture);
            k4a_image_t depth = k4a_capture_get_depth_image(capture);

            // --- Chama ---
            if (color)
                flame_engine.process(countFlamePixels(color));

            // --- Profundidade alinhada com a cor ---
            bool depthOk = false;
            if (depth)
                depthOk = (K4A_RESULT_SUCCEEDED ==
                           k4a_transformation_depth_image_to_color_camera(transformation, depth, depthInColor));

            // --- Corpo: placas ---
            k4abt_tracker_enqueue_capture(tracker, capture, 100);
            k4a_capture_release(capture);

            k4abt_frame_t body_frame = NULL;
            k4a_wait_result_t pop_frame_result = k4abt_tracker_pop_result(tracker, &body_frame, 100);

            if (pop_frame_result == K4A_WAIT_RESULT_SUCCEEDED) {
                uint32_t num_bodies = k4abt_frame_get_num_bodies(body_frame);
                SignAreas areas;   // 0 por defeito: sem corpo/dados, os contadores só descem (nunca envia nada)
                int dbgConf = -1;
                float dbgZ = 0.0f;

                if (num_bodies > 0) {
                    k4abt_body_t body;
                    k4abt_frame_get_body_skeleton(body_frame, 0, &body.skeleton);

                    k4a_float3_t shoulder = body.skeleton.joints[K4ABT_JOINT_SHOULDER_RIGHT].position;
                    dbgZ = shoulder.xyz.z;
                    dbgConf = (int)body.skeleton.joints[K4ABT_JOINT_SHOULDER_RIGHT].confidence_level;
                    bool confidence_ok =
                        body.skeleton.joints[K4ABT_JOINT_SHOULDER_RIGHT].confidence_level >= K4ABT_JOINT_CONFIDENCE_LOW;

                    if (confidence_ok && color && depthOk)
                        areas = signAreasInFrontOfShoulder(color, depthInColor, calibration, shoulder);
                }
                if (DEBUG_STATUS) {
                    static int dbgFrame = 0;
                    if (++dbgFrame % 10 == 0)
                        printf("[STATUS] corpos=%u ombroConf=%d ombroZ=%.0f cor=%d depth=%d | pxPerMm=%.2f frente=%d px "
                               "corMedia(RGB)=(%.0f,%.0f,%.0f) | vermelho=%.0f cm2 azul=%.0f cm2 %s\n",
                               num_bodies, dbgConf, dbgZ, color ? 1 : 0, depthOk ? 1 : 0,
                               areas.pxPerMm, areas.frontPx, areas.avgR, areas.avgG, areas.avgB,
                               areas.red, areas.blue, gesture_engine.paused ? "[PAUSADO]" : "");
                }
                gesture_engine.process(areas);
                k4abt_frame_release(body_frame);
            }

            if (color) k4a_image_release(color);
            if (depth) k4a_image_release(depth);
        }
    }

    k4a_image_release(depthInColor);
    k4a_transformation_destroy(transformation);
    k4abt_tracker_destroy(tracker);
    k4a_device_stop_cameras(device);
    k4a_device_close(device);
    return 0;
}