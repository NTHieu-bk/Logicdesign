#ifndef __TINY_ML__
#define __TINY_ML__

#include <Arduino.h>

#include "dht_anomaly_model.h"
#include "global.h"

#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

class TinyMLContext {
public:
    TinyMLContext();
    ~TinyMLContext();
    bool begin(const unsigned char *model_data);
    TfLiteStatus invoke();
    TfLiteTensor* getInput(int index = 0);
    TfLiteTensor* getOutput(int index = 0);
    tflite::ErrorReporter* getErrorReporter();
private:
    tflite::MicroErrorReporter micro_error_reporter_;
    tflite::ErrorReporter *error_reporter_;
    const tflite::Model *model_;
    tflite::MicroInterpreter *interpreter_;
    TfLiteTensor *input_;
    TfLiteTensor *output_;
    static tflite::AllOpsResolver resolver_;
    static constexpr int kTensorArenaSize = 8 * 1024;
    uint8_t tensor_arena_[kTensorArenaSize];
};

extern volatile int   current_ml_state;       // 0 = NORMAL, 1 = CHECK, 2 = WARNING
extern volatile float current_anomaly_score; 
extern volatile float current_anomaly_ratio;  
extern String         current_advice_msg;

// Environment classification (COLD / COMFORT / HOT)
extern volatile int   current_env_class;      
extern String         current_env_label;    

void tiny_ml_task(void *pvParameters);

#endif
