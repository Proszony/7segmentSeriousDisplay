#include "video.h"
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <array>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

using namespace cv;
using namespace std;

namespace {
    int fx(float k, int cell_w) { return std::max(1, int(k * cell_w + 0.5f)); }
    int fy(float k, int cell_h) { return std::max(1, int(k * cell_h + 0.5f)); }
}

cv::Size effective_resolution(cv::Size res, int rotation)
{
    if (rotation == 90 || rotation == 270)
        return cv::Size(res.height, res.width);
    return res;
}

std::vector<Cell> build_cells(cv::Size eff_res, cv::Size div)
{
    const int nx = div.width;
    const int ny = div.height;
    const int cell_w = eff_res.width / nx;
    const int cell_h = eff_res.height / ny;

    std::vector<Cell> cells;
    cells.reserve(static_cast<size_t>(nx) * static_cast<size_t>(ny));
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            cells.push_back({ cv::Point(i * cell_w, j * cell_h), cell_w, cell_h });
        }
    }
    return cells;
}

std::array<cv::Rect, 7> segment_rects(const Cell& c)
{
    return {{
        cv::Rect(c.origin.x + fx(0.20f, c.w), c.origin.y + fy(0.05f, c.h), fx(0.60f, c.w), fy(0.12f, c.h)), //A
        cv::Rect(c.origin.x + fx(0.78f, c.w), c.origin.y + fy(0.10f, c.h), fx(0.12f, c.w), fy(0.40f, c.h)), //B
        cv::Rect(c.origin.x + fx(0.78f, c.w), c.origin.y + fy(0.50f, c.h), fx(0.12f, c.w), fy(0.40f, c.h)), //C
        cv::Rect(c.origin.x + fx(0.20f, c.w), c.origin.y + fy(0.85f, c.h), fx(0.60f, c.w), fy(0.12f, c.h)), //D
        cv::Rect(c.origin.x + fx(0.10f, c.w), c.origin.y + fy(0.50f, c.h), fx(0.12f, c.w), fy(0.40f, c.h)), //E
        cv::Rect(c.origin.x + fx(0.10f, c.w), c.origin.y + fy(0.10f, c.h), fx(0.12f, c.w), fy(0.40f, c.h)), //F
        cv::Rect(c.origin.x + fx(0.20f, c.w), c.origin.y + fy(0.45f, c.h), fx(0.60f, c.w), fy(0.12f, c.h))  //G
    }};
}

void prepare_frame(cv::Mat& frame, cv::Mat& gray, cv::Size eff_res,
                   int rotation, bool invert)
{
    if      (rotation == 90)  rotate(frame, frame, ROTATE_90_CLOCKWISE);
    else if (rotation == 180) rotate(frame, frame, ROTATE_180);
    else if (rotation == 270) rotate(frame, frame, ROTATE_90_COUNTERCLOCKWISE);

    resize(frame, frame, eff_res);
    cvtColor(frame, gray, COLOR_BGR2GRAY);
    gray.convertTo(gray, CV_32F, 1.0/255.0);

    if (invert)
        gray = 1.0f - gray;
}

void process_frame_to_segstates(const cv::Mat& gray,
                                const std::vector<Cell>& cells,
                                float thresh,
                                std::vector<SegState>& segStates)
{
    if (segStates.size() != cells.size())
        segStates.resize(cells.size());

    const cv::Rect bounds(0, 0, gray.cols, gray.rows);

    for (size_t i = 0; i < cells.size(); ++i) {
        const auto rects = segment_rects(cells[i]);

        for (int seg = 0; seg < 7; ++seg) {
            const cv::Rect roi = rects[seg] & bounds;
            segStates[i].on[seg] = (roi.width > 0 && roi.height > 0 &&
                                    mean(gray(roi))[0] >= thresh);
        }
    }
}

void draw_segstates(cv::Mat& out,
                    const std::vector<Cell>& cells,
                    const std::vector<SegState>& segStates,
                    cv::Scalar color)
{
    for (size_t i = 0; i < cells.size(); ++i) {
        const auto rects = segment_rects(cells[i]);

        for (int seg = 0; seg < 7; ++seg) {
            if (segStates[i].on[seg])
                rectangle(out, rects[seg], color, FILLED);
        }
    }
}

void run_video(const string &filename,
               float thresh,
               bool invert_flag,
               Size res,
               Size div,
               float max_fps,
               int rotation,
               bool draw,
               Scalar seg_color,
               vector<SegState>& segStates)
{
    VideoCapture cap(filename);
    if(!cap.isOpened()) {
        cerr << "Error: Cannot open video: " << filename << endl;
        return;
    }
    
    if(max_fps <= 0) max_fps = cap.get(CAP_PROP_FPS);
    float frame_time = 1.0f / max_fps;

    Size eff_res = effective_resolution(res, rotation);

    Mat frame, gray, frame_out(eff_res, CV_8UC3);

    auto prev = chrono::high_resolution_clock::now();
    int frame_count = 0;

    vector<Cell> cells = build_cells(eff_res, div);
    segStates.resize(cells.size());

    if (draw) {
        namedWindow("7seg_binary", WINDOW_NORMAL | WINDOW_GUI_NORMAL);
        resizeWindow("7seg_binary", eff_res.width, eff_res.height);
    }

    while(true)
    {
        auto start = chrono::high_resolution_clock::now();

        if(!cap.read(frame))
            break;

        prepare_frame(frame, gray, eff_res, rotation, invert_flag);

        process_frame_to_segstates(gray, cells, thresh, segStates);

        if(draw){
            frame_out = Scalar(0,0,0);
            draw_segstates(frame_out, cells, segStates, seg_color);

            imshow("7seg_binary", frame_out);
            if(waitKey(1) == 27)
                break;
        }

        float elapsed = chrono::duration<float>(chrono::high_resolution_clock::now() - start).count();
        if(elapsed < frame_time)
            this_thread::sleep_for(chrono::duration<float>(frame_time - elapsed));

        frame_count++;
        auto now = chrono::high_resolution_clock::now();
        if(chrono::duration<float>(now - prev).count() >= 1.0f){
            cout << "\r\33[2KFPS: " << frame_count << flush;
            frame_count = 0;
            prev = now;
        }
    }
}

void display_frame(const Frame& f, Scalar seg_color) {
    static bool window_created = false;
    static Mat frame_out;

    int nx = f.max_X;
    int ny = f.max_Y;

    if (nx == 0 || ny == 0)
        return;

    int window_h = 480;
    int window_w = 640;
    int cell_w = window_w / nx;
    int cell_h = window_h / ny;

    Size res(nx * cell_w, ny * cell_h);

    if (!window_created) {
        namedWindow("7seg_receive", WINDOW_NORMAL);
        resizeWindow("7seg_receive", res.width, res.height);
        window_created = true;
    }

    frame_out = Mat(res, CV_8UC3, Scalar(0,0,0));

    for (int j = 0; j < ny; j++) {
        for (int i = 0; i < nx; i++) {
            int idx = j * nx + i;
            if (idx >= static_cast<int>(f.cells.size())) break;

            uint8_t b = f.cells[idx];
            Cell c{ Point(i * cell_w, j * cell_h), cell_w, cell_h };
            const auto rects = segment_rects(c);

            for (int seg = 0; seg < 7; seg++) {
                if (b & (1 << seg)) {
                    rectangle(frame_out, rects[seg], seg_color, FILLED);
                }
            }
        }
    }

    imshow("7seg_receive", frame_out);
    waitKey(1);
}
