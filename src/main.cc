#include <sys/stat.h>
#include <iostream>
#include <memory>
#include <vector>

#include "camerastreamer.h"
#include "inferencewrapper.h"

using coral::CameraStreamer;
using coral::InferenceWrapper;

#define LEAKY_Q " queue max-size-buffers=1 leaky=downstream "
const gchar* kPipeline =
    "v4l2src device=/dev/video1 !"
    "video/x-raw,framerate=30/1,width=640,height=480 ! " LEAKY_Q
    " ! tee name=t"
    " t. !" LEAKY_Q
    "! glimagesink"
    " t. !" LEAKY_Q
    "! videoscale ! video/x-raw,width=224,height=224 ! videoconvert ! "
    "video/x-raw,format=RGB ! appsink name=appsink";

// Callback function for frame processing
void interpret_frame(const uint8_t* pixels, int length, void* args) {
    std::cout << "Entering interpret_frame with pixel length: " << length << std::endl;
    if (pixels == nullptr || length <= 0) {
        std::cerr << "Invalid pixel data or length!" << std::endl;
        return;
    }
    
    InferenceWrapper* inferencer = reinterpret_cast<InferenceWrapper*>(args);
    if (inferencer == nullptr) {
        std::cerr << "InferenceWrapper is null!" << std::endl;
        return;
    }

    std::pair<std::string, float> results = inferencer->RunInference(pixels, length);
    std::cout << "Inference Result: " << results.first << " with confidence: " << results.second << std::endl;
}

void check_file(const char* file) {
    std::cout << "Checking file: " << file << std::endl;
    struct stat buf;
    if (stat(file, &buf) != 0) {
        std::cerr << file << " does not exist" << std::endl;
        exit(EXIT_FAILURE);
    }
}

void usage(char* argv[]) {
    std::cerr << "Usage: " << argv[0] << " --model model_file --labels label_file" << std::endl;
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
    std::cout << "Starting application..." << std::endl;
    
    std::string model_path;
    std::string label_path;

    if (argc == 5) {
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "--model") {
                model_path = argv[++i];
                std::cout << "Model path: " << model_path << std::endl;
            } else if (std::string(argv[i]) == "--labels") {
                label_path = argv[++i];
                std::cout << "Label path: " << label_path << std::endl;
            } else {
                usage(argv);
            }
        }
    } else {
        usage(argv);
    }

    check_file(model_path.c_str());
    check_file(label_path.c_str());

    std::cout << "Initializing InferenceWrapper..." << std::endl;
    InferenceWrapper inferencer(model_path, label_path);

    std::cout << "Initializing CameraStreamer..." << std::endl;
    coral::CameraStreamer streamer;

    std::cout << "Starting pipeline..." << std::endl;
    streamer.RunPipeline(kPipeline, {interpret_frame, &inferencer});
    
    std::cout << "Exiting application..." << std::endl;
}
