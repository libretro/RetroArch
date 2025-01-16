#include "RetroModel.h"


//=======================================================
// RetroModelPytorch::LoadModel
//=======================================================
void RetroModelPytorch::LoadModel(std::string path)
{
  try {
    this->module = torch::jit::load(path);
    std::cerr << "LOADED MODEL:!" << path << std::endl;
  }
  catch (const c10::Error& e) {
    std::cerr << "error loading the model\n";
    return;
  }
}

//=======================================================
// RetroModelPytorch::Forward
//=======================================================
void RetroModelPytorch::Forward(std::vector<float> & output, const std::vector<float> & input)
{
    std::vector<torch::jit::IValue> inputs;

    at::Tensor tmp = torch::zeros({1, input.size()});

    for(int i=0; i < input.size(); i++)
    {
        tmp[0][i] = input[i];
    }

    inputs.push_back(tmp);
    
    at::Tensor result = module.forward(inputs).toTuple()->elements()[0].toTensor();

    for(int i=0; i < output.size(); i++)
    {
        output[i] = result[0][i].item<float>();
    }
}

//=======================================================
// RetroModelPytorch::Forward
//=======================================================
void RetroModelPytorch::Forward(std::vector<float> & output, RetroModelFrameData & input)
{
    std::vector<torch::jit::IValue> inputs;

    cv::Mat image(cv::Size(input.width, input.height), CV_8UC2, input.data);
    cv::Mat rgb;  
    cv::Mat gray;
    cv::Mat result;

    // add new frame on the stack
    cv::Mat * newFrame = input.PushNewFrameOnStack();
    
    // Downsample to 84x84 and turn to greyscale
    cv::cvtColor(image, gray, cv::COLOR_BGR5652GRAY);
    cv::resize(gray, result, cv::Size(84,84), cv::INTER_AREA);
    result.copyTo(*newFrame);

    /*cv::namedWindow("Display Image", cv::WINDOW_NORMAL);
    cv::imshow("Display Image", result);
    cv::waitKey(0);*/

    at::Tensor tmp = torch::ones({1, 4, 84, 84});

    for(auto i : {0,1,2,3})
    {
      if(input.stack[i]->data)
        tmp[0][3-i] = torch::from_blob(input.stack[i]->data, { result.rows, result.cols }, at::kByte);
    }

 
    /*test[0][3] = torch::from_blob(input.stack[0]->data, { result.rows, result.cols }, at::kByte);
    if(input.stack[1]->data)
      test[0][2] = torch::from_blob(input.stack[1]->data, { result.rows, result.cols }, at::kByte);
    if(input.stack[2]->data)
      test[0][1] = torch::from_blob(input.stack[2]->data, { result.rows, result.cols }, at::kByte);
    if(input.stack[3]->data)
      test[0][0] = torch::from_blob(input.stack[3]->data, { result.rows, result.cols }, at::kByte);*/

    inputs.push_back(tmp);

    // Execute the model and turn its output into a tensor.
    torch::jit::IValue ret = module.forward(inputs);
    at::Tensor actions = ret.toTuple()->elements()[0].toTensor();

    for(int i=0; i < output.size(); i++)
    {
        output[i] = actions[0][i].item<float>();
    }
}