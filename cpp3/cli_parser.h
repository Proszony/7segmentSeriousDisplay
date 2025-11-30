#pragma once
#include <string>
#include <opencv2/opencv.hpp>

struct Params {
    std::string filename = "BadApple!!.mp4";
    float thresh = 0.5f;
    bool invert_flag = false;
    bool audio_flag = false;
    bool draw = false;
    float max_fps = 30.0f;
    cv::Size res = cv::Size(640, 360);
    cv::Size div = cv::Size(32, 12);
    cv::Scalar seg_color = cv::Scalar(0, 0, 255);
};

Params parse_args(int argc, char** argv);
void print_help();

