#include "inferencewrapper.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "tensorflow/lite/builtin_op_data.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "edgetpu.h"

#define DEBUG_ON false  // Change to true to enable debug messages

#define DEBUG_LOG(msg) \
    do { if (DEBUG_ON) std::cout << "[DEBUG] " << msg << std::endl; } while (0)

#define TFLITE_MINIMAL_CHECK(x)                              \
    if (!(x)) {                                              \
        fprintf(stderr, "Error at %s:%d\n", __FILE__, __LINE__); \
        exit(EXIT_FAILURE);                                  \
    }

namespace coral {
    namespace {
        // Helper function to read labels from a file
        std::vector<std::string> read_labels(const std::string &label_path) {
            DEBUG_LOG("Reading labels from file: " << label_path);
            std::vector<std::string> labels;

            std::ifstream label_file(label_path);
            std::string line;
            if (label_file.is_open()) {
                while (getline(label_file, line)) {
                    labels.push_back(line);
                }
            } else {
                std::cerr << "[ERROR] Unable to open file: " << label_path << std::endl;
                exit(EXIT_FAILURE);
            }

            DEBUG_LOG("Successfully read " << labels.size() << " labels.");
            return labels;
        }
    } // namespace

    InferenceWrapper::InferenceWrapper(const std::string &model_path, const std::string &label_path) {
        DEBUG_LOG("Initializing InferenceWrapper with model: " << model_path
                  << " and labels: " << label_path);

        // Load model
        auto model = tflite::FlatBufferModel::BuildFromFile(model_path.c_str());
        TFLITE_MINIMAL_CHECK(model != nullptr);
        DEBUG_LOG("Model loaded successfully.");

        // Open Edge TPU device
        edgetpu_context_ = edgetpu::EdgeTpuManager::GetSingleton()->OpenDevice();
        if (!edgetpu_context_) {
            std::cerr << "[ERROR] Failed to open Edge TPU device." << std::endl;
            exit(EXIT_FAILURE);
        }
        DEBUG_LOG("Edge TPU device opened successfully.");

        // Build interpreter
        tflite::ops::builtin::BuiltinOpResolver resolver;
        resolver.AddCustom(edgetpu::kCustomOp, edgetpu::RegisterCustomOp());
        if (tflite::InterpreterBuilder(*model, resolver)(&interpreter_) != kTfLiteOk) {
            std::cerr << "[ERROR] Failed to build Interpreter." << std::endl;
            exit(EXIT_FAILURE);
        }
        DEBUG_LOG("Interpreter built successfully.");

        // Set TPU context
        interpreter_->SetExternalContext(kTfLiteEdgeTpuContext, edgetpu_context_.get());
        interpreter_->SetNumThreads(1);

        // Allocate tensors
        TFLITE_MINIMAL_CHECK(interpreter_->AllocateTensors() == kTfLiteOk);
        DEBUG_LOG("Tensors allocated successfully.");

        // Load labels
        labels_ = read_labels(label_path);
        DEBUG_LOG("InferenceWrapper initialized successfully.");
    }

    std::pair<std::string, float> InferenceWrapper::RunInference(
        const uint8_t *input_data, int input_size) {
        DEBUG_LOG("Running inference with input size: " << input_size);

        // Prepare input
        uint8_t *input = interpreter_->typed_input_tensor<uint8_t>(0);
        if (!input) {
            std::cerr << "[ERROR] Input tensor is null." << std::endl;
            exit(EXIT_FAILURE);
        }
        std::memcpy(input, input_data, input_size);
        DEBUG_LOG("Input data copied to tensor.");

        // Invoke inference
        TFLITE_MINIMAL_CHECK(interpreter_->Invoke() == kTfLiteOk);
        DEBUG_LOG("Inference invoked successfully.");

        // Process output
        const auto &output_indices = interpreter_->outputs();
        const auto *out_tensor = interpreter_->tensor(output_indices[0]);
        TFLITE_MINIMAL_CHECK(out_tensor != nullptr);
        DEBUG_LOG("Output tensor retrieved successfully.");

        float max_prob = 0.0f;
        int max_index = -1;
        if (out_tensor->type == kTfLiteUInt8) {
            const uint8_t *output = interpreter_->typed_output_tensor<uint8_t>(0);
            max_index = std::max_element(output, output + out_tensor->bytes) - output;
            max_prob = (output[max_index] - out_tensor->params.zero_point) * out_tensor->params.scale;
            DEBUG_LOG("Processed UINT8 output tensor.");
        } else if (out_tensor->type == kTfLiteFloat32) {
            const float *output = interpreter_->typed_output_tensor<float>(0);
            max_index = std::max_element(output, output + out_tensor->bytes / sizeof(float)) - output;
            max_prob = output[max_index];
            DEBUG_LOG("Processed FLOAT32 output tensor.");
        } else {
            std::cerr << "[ERROR] Tensor " << out_tensor->name
                      << " has unsupported output type: " << out_tensor->type << std::endl;
            exit(EXIT_FAILURE);
        }

        std::cout << "[INFO] Inference result: " << labels_[max_index]
                  << " with confidence: " << max_prob << std::endl;
        return {labels_[max_index], max_prob};
    }

} // namespace coral
