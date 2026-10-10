#include <iostream>
#include <cmath>
#include <algorithm>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <k4a/k4a.h>
#include <k4abt.h>

// ---- STOP (braço esticado) ----
const float STOP_FORWARD_MM     = 350.0f;  // quanto a mão tem de estar à frente do ombro
const float STOP_HEIGHT_TOL     = 150.0f;  // tolerância vertical mão/ombro (mm)
const int   STOP_HOLD_FRAMES    = 15;      // ~0,5 s a 30 fps
const int   STOP_RELEASE_FRAMES = 6;

// ---- CHAMA (vela / isqueiro) ----
const int FLAME_MIN_PIXELS  = 8;   // para acender
const int FLAME_KEEP_PIXELS = 3;   // para manter acesa
const int FLAME_ON_FRAMES   = 3;   // frames seguidos acima de MIN para ligar (anti-ruído)
const int FLAME_OFF_FRAMES  = 10;  // frames seguidos abaixo de KEEP para desligar (~0,3 s, a chama tremeluz)

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

// ===================== STOP =====================
struct GestureTracker {
    int holdFrames = 0;
    bool stopActive = false;
    int releaseFrames = 0;

    void onLost() {
        if (stopActive) {            // não deixar o jogo preso em pausa
            g_udp.send("RESUME");
            stopActive = false;
        }
        holdFrames = 0;
        releaseFrames = 0;
    }

    void process(const k4a_float3_t& hand, const k4a_float3_t& shoulder) {
        float forward = shoulder.xyz.z - hand.xyz.z;
        float heightDiff = fabsf(hand.xyz.y - shoulder.xyz.y);
        bool armExtended = (forward > STOP_FORWARD_MM) && (heightDiff < STOP_HEIGHT_TOL);

        if (stopActive) {
            // Já em pausa: só verifica se o braço baixou
            if (!armExtended) {
                releaseFrames++;
                if (releaseFrames >= STOP_RELEASE_FRAMES) {
                    std::cout << "\n>>> [GESTURE] STOP ENDED (RESUME) <<<\n";
                    g_udp.send("RESUME");
                    stopActive = false;
                    releaseFrames = 0;
                }
            } else {
                releaseFrames = 0;
            }
            return;
        }

        if (armExtended) {
            holdFrames++;
            if (holdFrames >= STOP_HOLD_FRAMES) {
                std::cout << "\n>>> [GESTURE] STOP (PAUSE) <<<\n";
                g_udp.send("PAUSE");
                stopActive = true;
                holdFrames = 0;
                releaseFrames = 0;
            }
        } else {
            holdFrames = 0;
        }
    }
};

// ===================== CHAMA =====================
// Detecta pixels de chama na imagem de cor (BGRA)
// Critério: Vermelho forte, Verde médio, Azul fraco, e R > G + 40
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
                                K4A_COLOR_CONTROL_MODE_MANUAL, 2500); // µs; valores típicos: 500, 1250, 2500, 8330
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

    GestureTracker gesture_engine;
    FlameTracker flame_engine;
    std::cout << "Tracker running. Estica o braco para PAUSE; acende uma vela/isqueiro para ligar as velas.\n";

    while (true) {
        k4a_capture_t capture = NULL;
        k4a_wait_result_t get_capture_result = k4a_device_get_capture(device, &capture, 100);

        if (get_capture_result == K4A_WAIT_RESULT_SUCCEEDED) {

            // --- Chama: usa a imagem de cor deste capture (se existir) ---
            k4a_image_t color = k4a_capture_get_color_image(capture);
            if (color) {
                flame_engine.process(countFlamePixels(color));
                k4a_image_release(color);
            }

            // --- Corpo: STOP ---
            k4abt_tracker_enqueue_capture(tracker, capture, 100);
            k4a_capture_release(capture);

            k4abt_frame_t body_frame = NULL;
            k4a_wait_result_t pop_frame_result = k4abt_tracker_pop_result(tracker, &body_frame, 100);

            if (pop_frame_result == K4A_WAIT_RESULT_SUCCEEDED) {
                uint32_t num_bodies = k4abt_frame_get_num_bodies(body_frame);

                if (num_bodies > 0) {
                    k4abt_body_t body;
                    k4abt_frame_get_body_skeleton(body_frame, 0, &body.skeleton);

                    k4a_float3_t hand = body.skeleton.joints[K4ABT_JOINT_HAND_RIGHT].position;
                    k4a_float3_t shoulder = body.skeleton.joints[K4ABT_JOINT_SHOULDER_RIGHT].position;
                    k4abt_joint_confidence_level_t hand_conf = body.skeleton.joints[K4ABT_JOINT_HAND_RIGHT].confidence_level;
                    k4abt_joint_confidence_level_t shoulder_conf = body.skeleton.joints[K4ABT_JOINT_SHOULDER_RIGHT].confidence_level;
                    bool confidence_ok = hand_conf >= K4ABT_JOINT_CONFIDENCE_LOW && shoulder_conf >= K4ABT_JOINT_CONFIDENCE_LOW;

                    if (confidence_ok) gesture_engine.process(hand, shoulder);
                    else               gesture_engine.onLost();
                }
                k4abt_frame_release(body_frame);
            }
        }
    }

    k4abt_tracker_destroy(tracker);
    k4a_device_stop_cameras(device);
    k4a_device_close(device);
    return 0;
}