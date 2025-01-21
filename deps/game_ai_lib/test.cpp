// test of game ai dynamic lib
#include <iostream>
#include <assert.h>
#include <filesystem>
#include <stdexcept>
#include <opencv2/opencv.hpp>
#include <torch/script.h>
#include "GameAI.h"
#include "RetroModel.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif



#if 0
/*
#include "onnxruntime_cxx_api.h"

void Test_ONNX()
{

// Load the model and create InferenceSession
Ort::Env env;
std::string model_path = "path/to/your/onnx/model";
Ort::Session session(env, model_path, Ort::SessionOptions{ nullptr });
// Load and preprocess the input image to inputTensor
...
// Run inference
std::vector outputTensors =
session.Run(Ort::RunOptions{nullptr}, inputNames.data(), &inputTensor,
  inputNames.size(), outputNames.data(), outputNames.size());
const float* outputDataPtr = outputTensors[0].GetTensorMutableData();
std::cout << outputDataPtr[0] << std::endl;


}*/

void Test_Resnet()
{

    torch::jit::script::Module module;
try {

    module = torch::jit::load("/home/mat/github/stable-retro-scripts/traced_resnet_model.pt");
    //module = torch::jit::load("/home/mat/github/stable-retro-scripts/model.pt");
    std::cerr << "SUCCESS!\n";

    module.eval();

    // Create a vector of inputs.
    std::vector<torch::jit::IValue> inputs;
    inputs.push_back(torch::ones({1, 3, 224, 224}));
    //inputs.push_back(torch::ones({1, 4, 84, 84}));

    // Execute the model and turn its output into a tensor.
    at::Tensor output = module.forward(inputs).toTensor();
    //std::cout << output.slice(/*dim=*/1, /*start=*/0, /*end=*/5) << '\n';
  }
  catch (const c10::Error& e) {
    std::cerr << "error loading the model\n";
    return;
  }
}
#endif

//=======================================================
// test_opencv
//=======================================================
void test_opencv(std::map<std::string, bool> & tests)
{
    cv::Mat image;
    cv::Mat grey;
    cv::Mat result;

    image = cv::imread( "../screenshots/wwf.png", cv::IMREAD_COLOR );

    cv::cvtColor(image, grey, cv::COLOR_RGB2GRAY);
    cv::resize(grey, result, cv::Size(84,84), cv::INTER_AREA);

    if ( !image.data )
    {
        printf("No image data \n");
        return;
    }
    cv::namedWindow("Display Image", cv::WINDOW_NORMAL);
    cv::imshow("Display Image", result);

    cv::waitKey(1000);

    tests["OPENCV GRAYSCALE DOWNSAMPLE TO 84x84"] = true;
}

//=======================================================
// test_loadlibrary
//=======================================================
void test_loadlibrary(std::map<std::string, bool> & tests)
{
    GameAI * ga = nullptr;
    create_game_ai_t func = nullptr;

#ifdef _WIN32
    HINSTANCE hinstLib;
    BOOL fFreeResult, fRunTimeLinkSuccess = FALSE;

    hinstLib = LoadLibrary(TEXT("game_ai.dll"));

    if (hinstLib != NULL)
    {
        tests["LOAD LIBRARY"] = true;
        func  = (create_game_ai_t) GetProcAddress(hinstLib, "create_game_ai");
    }
#else
    void *myso = dlopen("./libgame_ai.so", RTLD_NOW);

    //std::cout << dlerror() << std::endl;

    if(myso)
    {
        tests["LOAD LIBRARY"] = true;

        func = reinterpret_cast<create_game_ai_t>(dlsym(myso, "create_game_ai"));
    }
#endif
        if(func)
        {
            tests["GET CREATEGAME FUNC"] = true;
            ga = (GameAI *) func("./data/NHL941on1-Genesis/NHL941on1.md");

            if(ga)
                tests["CREATEGAME FUNC"] = true;
        }

#ifdef _WIN32
    fFreeResult = FreeLibrary(hinstLib);
#endif
}

//=======================================================
// test_pytorch
//=======================================================
void test_pytorch(std::map<std::string, bool> & tests)
{

try {
    RetroModelPytorch * model = new RetroModelPytorch();

    model->LoadModel(std::string("./data/NHL941on1-Genesis/ScoreGoal.pt"));

    std::vector<float> input(16);
    std::vector<float> output(12);

    model->Forward(output, input);

    //TODO validate output
    tests["LOAD PYTORCH MODEL"] = true;

  }
  catch (const c10::Error& e) {
    //std::cerr << "error loading the model\n";
    throw std::runtime_error ("error loading the model\n");
    return;
  }
}

int main()
{
    std::map<std::string, bool> tests;

    tests.insert(std::pair<std::string, bool>("LOAD LIBRARY",false));
    tests.insert(std::pair<std::string, bool>("GET CREATEGAME FUNC",false));
    tests.insert(std::pair<std::string, bool>("CREATEGAME FUNC",false));
    tests.insert(std::pair<std::string, bool>("OPENCV GRAYSCALE DOWNSAMPLE TO 84x84",false));
    tests.insert(std::pair<std::string, bool>("LOAD PYTORCH MODEL",false));

    std::cout << "========== RUNNING TESTS ==========" << std::endl;

    try {
        test_loadlibrary(tests);

        test_opencv(tests);

        test_pytorch(tests);
    }
    catch (std::exception &e) {
        std::cout << "============= EXCEPTION =============" << std::endl;
        std::cout << e.what();
    }

    std::cout << "============== RESULTS =============" << std::endl;

    for(auto i: tests)
    {
        const char * result = i.second ? "PASS" : "FAIL";
        std::cout << i.first << "..." << result << std::endl;
    }

    return 0;
}