#include <iostream>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <k4a/k4a.h>
#include <k4abt.h>

const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;

const float X_MIN = -250.0f; 
const float X_MAX =  250.0f; 
const float Y_MIN = -150.0f; 
const float Y_MAX =  250.0f; 

const float PUSH_DEPTH_THRESHOLD = 40.0f;    
const float SWIPE_VELOCITY_THRESHOLD = 50.0f; 
const int GESTURE_COOLDOWN_FRAMES = 15;      

struct GestureTracker {
    float prev_x = 0.0f;
    float prev_z = 0.0f;
    bool has_prev = false;
    int cooldown = 0;

    void process(float x, float y, float z) {
        float norm_x = (x - X_MIN) / (X_MAX - X_MIN);
        float norm_y = (y - Y_MIN) / (Y_MAX - Y_MIN);

        norm_x = std::max(0.0f, std::min(1.0f, norm_x));
        norm_y = std::max(0.0f, std::min(1.0f, norm_y));

        int cursor_x = static_cast<int>(norm_x * SCREEN_WIDTH);
        int cursor_y = static_cast<int>((1.0f - norm_y) * SCREEN_HEIGHT);

        std::cout << "Cursor: (" << cursor_x << ", " << cursor_y << ") | Hand Z: " << z << " mm\r" << std::flush;

        if (!has_prev) {
            prev_x = x;
            prev_z = z;
            has_prev = true;
            return;
        }

        if (cooldown > 0) {
            cooldown--;
            prev_x = x;
            prev_z = z;
            return;
        }

        float delta_z = prev_z - z; 
        if (delta_z > PUSH_DEPTH_THRESHOLD) {
            std::cout << "\n>>> [GESTURE] PUSH CLICK at (" << cursor_x << ", " << cursor_y << ") <<<\n";
            cooldown = GESTURE_COOLDOWN_FRAMES;
        }

        float delta_x = x - prev_x;
        if (delta_x > SWIPE_VELOCITY_THRESHOLD) {
            std::cout << "\n>>> [GESTURE] SWIPE RIGHT <<<\n";
            cooldown = GESTURE_COOLDOWN_FRAMES;
        } else if (delta_x < -SWIPE_VELOCITY_THRESHOLD) {
            std::cout << "\n>>> [GESTURE] SWIPE LEFT <<<\n";
            cooldown = GESTURE_COOLDOWN_FRAMES;
        }

        prev_x = x;
        prev_z = z;
    }
};

int main() {
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
                    k4abt_joint_confidence_level_t confidence = body.skeleton.joints[K4ABT_JOINT_HAND_RIGHT].confidence_level;

                    if (confidence >= K4ABT_JOINT_CONFIDENCE_LOW) {
                        gesture_engine.process(hand.xyz.x, hand.xyz.y, hand.xyz.z);
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