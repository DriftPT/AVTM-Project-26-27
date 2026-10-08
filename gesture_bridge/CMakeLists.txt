cmake_minimum_required(VERSION 3.15)
project(FemtoGesturesUI CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- SDK Directories for Headers and Libraries ---
set(ORBBEC_K4A_DIR "C:/dev/OrbbecSDK_K4A_Wrapper/OrbbecSDK_K4A_Wrapper_v2.0.12_windows_202605090937")
set(AZURE_K4ABT_DIR "C:/Program Files/Azure Kinect Body Tracking SDK")
set(VERIFIED_TOOLS_DIR "${AZURE_K4ABT_DIR}/tools")

# 1. Compiler Includes (Must point to both)
include_directories(
    "${ORBBEC_K4A_DIR}/include"
    "${AZURE_K4ABT_DIR}/sdk/include"
)

# 2. Linker Search Directories (Must point to both)
link_directories(
    "${ORBBEC_K4A_DIR}/lib/"
    "${AZURE_K4ABT_DIR}/sdk/windows-desktop/amd64/release/lib"
)

# 3. Target Definition
add_executable(GestureApp main.cpp)

target_link_libraries(GestureApp PRIVATE
    k4a
    k4abt
)

# 4. Post-Build: Deploy all verified runtime files from the tools directory
add_custom_command(TARGET GestureApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VERIFIED_TOOLS_DIR}/k4a.dll"
        "${VERIFIED_TOOLS_DIR}/OrbbecSDK.dll"
        "${VERIFIED_TOOLS_DIR}/depthengine_2_0.dll"
        "${VERIFIED_TOOLS_DIR}/k4arecord.dll"
        "${VERIFIED_TOOLS_DIR}/k4abt.dll"
        "${VERIFIED_TOOLS_DIR}/onnxruntime.dll"
        "${VERIFIED_TOOLS_DIR}/dnn_model_2_0_op11.onnx"
        $<TARGET_FILE_DIR:GestureApp>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${VERIFIED_TOOLS_DIR}/extensions"
        "$<TARGET_FILE_DIR:GestureApp>/extensions"
    COMMENT "Deploying verified runtime DLLs, ONNX models, and extensions folder to build directory..."
)