#include <iostream>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <k4a/k4a.h>
#include <k4abt.h>

const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;

const float X_MIN = -250.0f; 
const float X_MAX =  250.0f; 
const float Y_MIN = -150.0f; 
const float Y_MAX =  250.0f; 

const float SWIPE_VELOCITY_THRESHOLD = 50.0f;  // mm por frame (30 fps)
const int   GESTURE_COOLDOWN_FRAMES = 30;  // ~1 s: cobre o retorno da mão

const float STOP_FORWARD_MM   = 350.0f;  // quanto a mão tem de estar à frente do ombro
const float STOP_HEIGHT_TOL   = 150.0f;  // tolerância vertical mão/ombro (mm)
const int   STOP_HOLD_FRAMES  = 15;      // ~0,5 s a 30 fps
const int   STOP_RELEASE_FRAMES = 6;

struct UdpSender {
    SOCKET s = INVALID_SOCKET;
    sockaddr_in addr{};

    bool init(const char* ip, unsigned short port) {
        WSADATA w;
        if (WSAStartup(MAKEWORD(2, 2), &w) != 0) return false;
        s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET) return false;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
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

struct GestureTracker {
    float prev_x = 0.0f;
    bool has_prev = false;
    int cooldown = 0;
    int holdFrames = 0;
    bool stopActive = false;
    int releaseFrames = 0;

    void process(const k4a_float3_t& hand, const k4a_float3_t& shoulder) {
        float x = hand.xyz.x;
        if (!has_prev) {
            prev_x = x;
            has_prev = true;
            return;
        }

        // --- Estado do STOP (avaliado sempre, mesmo em cooldown) ---
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
                    cooldown = GESTURE_COOLDOWN_FRAMES;
                }
            } else {
                releaseFrames = 0;
            }
            return;   // enquanto o stop está ativo, ignora swipes
        }

        if (cooldown > 0) {
            cooldown--;
            prev_x = x;
            holdFrames = 0;
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
            prev_x = x;
            return;
        }
        holdFrames = 0;

        // --- SWIPES (janela de tempo) ---
        float delta_x = x - prev_x;
        if (delta_x < -SWIPE_VELOCITY_THRESHOLD) {
            std::cout << "\n>>> [GESTURE] SWIPE RIGHT -> camara seguinte <<<\n";
            g_udp.send("CAM_NEXT");
            cooldown = GESTURE_COOLDOWN_FRAMES;
        } else if (delta_x > SWIPE_VELOCITY_THRESHOLD) {
            std::cout << "\n>>> [GESTURE] SWIPE LEFT -> camara anterior <<<\n";
            g_udp.send("CAM_PREV");
            cooldown = GESTURE_COOLDOWN_FRAMES;
        }

        prev_x = x;
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
    config.color_resolution = K4A_COLOR_RESOLUTION_OFF;
    config.camera_fps = K4A_FRAMES_PER_SECOND_30;

    if (K4A_RESULT_SUCCEEDED != k4a_device_start_cameras(device, &config)) {
        std::cerr << "Failed to start camera streams.\n";
        k4a_device_close(device);
        return -1;
    }

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
    std::cout << "Tracker running. Stand 1.5m - 2.5m away and move your right hand.\n";

    while (true) {
        k4a_capture_t capture = NULL;
        k4a_wait_result_t get_capture_result = k4a_device_get_capture(device, &capture, 100);

        if (get_capture_result == K4A_WAIT_RESULT_SUCCEEDED) {
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
                    k4abt_joint_confidence_level_t confidence = body.skeleton.joints[K4ABT_JOINT_HAND_RIGHT].confidence_level;

                    if (confidence >= K4ABT_JOINT_CONFIDENCE_LOW) {
                        gesture_engine.process(hand, shoulder);
                    }
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