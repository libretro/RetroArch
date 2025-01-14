#pragma once

#include <torch/script.h>
#include <opencv2/opencv.hpp>
#include <bitset>
#include <string>
#include <filesystem>
#include <vector>
#include <queue>

class RetroModelFrameData
{
public:
        RetroModelFrameData(): data(nullptr)
        {
            stack[0] = new cv::Mat;
            stack[1] = new cv::Mat;
            stack[2] = new cv::Mat;
            stack[3] = new cv::Mat;
        }

        ~RetroModelFrameData()
        {
            if(stack[0]) delete stack[0];
            if(stack[1]) delete stack[1];
            if(stack[2]) delete stack[2];
            if(stack[3]) delete stack[3];
        }

        cv::Mat * PushNewFrameOnStack()
        {
            //push everything down
            cv::Mat * tmp = stack[3];
            stack[3] = stack[2];
            stack[2] = stack[1];
            stack[1] = stack[0];
            stack[0] = tmp;

            return stack[0];
        }
        

        void *data;
        unsigned int width;
        unsigned int height;
        unsigned int pitch;
        unsigned int format;

        cv::Mat * stack[4];
};

class RetroModel {
public:
        virtual void Forward(std::vector<float> & output, const std::vector<float> & input)=0;
        virtual void Forward(std::vector<float> & output, RetroModelFrameData & input)=0;
};

class RetroModelPytorch : public RetroModel {
public:
        virtual void LoadModel(std::string);
        virtual void Forward(std::vector<float> & output, const std::vector<float> & input);
        virtual void Forward(std::vector<float> & output, RetroModelFrameData & input);

private:
    torch::jit::script::Module module;
};



