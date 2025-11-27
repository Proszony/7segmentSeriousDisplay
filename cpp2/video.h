#pragma once

#include <opencv2/opencv.hpp>
#include <string>

void run_video(const std::string &filename,
               float thresh,
               bool invert_flag,
               cv::Size res,
               cv::Size div,
               float max_fps,
	       bool draw);
