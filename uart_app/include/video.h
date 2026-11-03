#pragma once

#include <array>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

#include "framing.h"

struct Cell {
    cv::Point origin;
    int w, h;
};

cv::Size effective_resolution(cv::Size res, int rotation);

std::vector<Cell> build_cells(cv::Size eff_res, cv::Size div);

std::array<cv::Rect, 7> segment_rects(const Cell& c);

void prepare_frame(cv::Mat& frame, cv::Mat& gray, cv::Size eff_res,
                   int rotation, bool invert);

void process_frame_to_segstates(const cv::Mat& gray,
                                const std::vector<Cell>& cells,
                                float thresh,
                                std::vector<SegState>& segStates);

void draw_segstates(cv::Mat& out,
                    const std::vector<Cell>& cells,
                    const std::vector<SegState>& segStates,
                    cv::Scalar color);

void run_video(const std::string& filename,
               float thresh,
               bool invert_flag,
               cv::Size res,
               cv::Size div,
               float max_fps,
               int rotation,
               bool draw,
               cv::Scalar seg_color,
               std::vector<SegState>& segStates);

void display_frame(const Frame& f, cv::Scalar seg_color);
