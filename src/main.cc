#include <sys/stat.h>
#include <iostream>
#include <memory>
#include <vector>

#include "camerastreamer.h"
#include "inferencewrapper.h"

using coral::CameraStreamer;
using coral::InferenceWrapper;

// Enable or disable debug logging
#define DEBUG_ON false  // Change this to true to enable debug logs

#define DEBUG_LOG(msg) \
    do { if (DEBUG_ON) std::cout << "[DEBUG] " << msg << std::endl; } while (0)

#define LEAKY_Q " queue max-size-buffers=1 leaky=downstream "

// Callback function for frame processing
void interpret_frame(const uint8_t* pixels, int length, void* args) {
    DEBUG_LOG("Entering interpret_frame with pixel length: " << length);
    if (pixels == nullptr || length <= 0) {
        std::cerr << "[ERROR] Invalid pixel data or length!" << std::endl;
        return;
    }
    
    InferenceWrapper* inferencer = reinterpret_cast<InferenceWrapper*>(args);
    if (inferencer == nullptr) {
        std::cerr << "[ERROR] InferenceWrapper is null!" << std::endl;
        return;
    }

    std::pair<std::string, float> results = inferencer->RunInference(pixels, length);
    DEBUG_LOG("Inference Result: " << results.first << " with confidence: " << results.second);
}

void check_file(const char* file) {
    DEBUG_LOG("Checking file: " << file);
    struct stat buf;
    if (stat(file, &buf) != 0) {
        std::cerr << "[ERROR] " << file << " does not exist" << std::endl;
        exit(EXIT_FAILURE);
    }
}

void usage(char* argv[]) {
    std::cerr << "[ERROR] Usage: " << argv[0] << " --model model_file --labels label_file --device /dev/video1" << std::endl;
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
    std::cout << "[INFO] Starting application..." << std::endl;
    
    std::string model_path;
    std::string label_path;
    std::string video_device;

    if (argc == 7) {
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "--model") {
                model_path = argv[++i];
                DEBUG_LOG("Model path: " << model_path);
            } else if (std::string(argv[i]) == "--labels") {
                label_path = argv[++i];
                DEBUG_LOG("Label path: " << label_path);
            } else if (std::string(argv[i]) == "--device") {
                video_device = argv[++i];
                DEBUG_LOG("Video device: " << video_device);
            } else {
                usage(argv);
            }
        }
    } else {
        usage(argv);
    }

    check_file(model_path.c_str());
    check_file(label_path.c_str());

    std::cout << "[INFO] Initializing InferenceWrapper..." << std::endl;
    InferenceWrapper inferencer(model_path, label_path);

    std::cout << "[INFO] Initializing CameraStreamer..." << std::endl;
    coral::CameraStreamer streamer;

    // Construct pipeline dynamically with user-specified video device
    std::string gstreamer_pipeline = 
        "v4l2src device=" + video_device + " ! image/jpeg,framerate=30/1,width=640,height=480 ! "
        "jpegdec ! videoconvert ! video/x-raw,format=BGRx ! " LEAKY_Q
        " ! tee name=t "
        " t. !" LEAKY_Q " ! ximagesink "
        " t. !" LEAKY_Q " ! videoscale ! video/x-raw,width=224,height=224 ! videoconvert ! "
        "video/x-raw,format=RGB ! appsink name=appsink";

    DEBUG_LOG("GStreamer pipeline: " << gstreamer_pipeline);

    std::cout << "[INFO] Starting GStreamer pipeline..." << std::endl;
    streamer.RunPipeline(gstreamer_pipeline.c_str(), {interpret_frame, &inferencer});
    
    std::cout << "[INFO] Exiting application..." << std::endl;
}
