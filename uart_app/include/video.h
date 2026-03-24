#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

#include "framing.h"

void run_video(const std::string& filename,
               float thresh,
               bool invert_flag,
               cv::Size res,
               cv::Size div,
               float max_fps,
               bool draw,
               cv::Scalar seg_color,
               std::vector<SegState>& segStates);

void display_frame(const Frame& f, cv::Scalar seg_color);