#include "video.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

using namespace cv;
using namespace std;

void run_video(const string &filename,
               float thresh,
               bool invert_flag,
               Size res,
               Size div,
               float max_fps,
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

    Mat frame, gray, frame_out(res, CV_8UC3);

    auto prev = chrono::high_resolution_clock::now();
    int frame_count = 0;

    int nx = div.width;
    int ny = div.height;

    int cell_w = res.width / nx;
    int cell_h = res.height / ny;

    struct Cell{
        Point origin;
        int w, h;
    };
    vector<Cell> cells;
    for(int j = 0; j < ny; j++){
        for(int i = 0; i < nx; i++){
            cells.push_back({ Point(i * cell_w, j * cell_h), cell_w, cell_h });
        }
    }

    segStates.resize(cells.size());

    if (draw) {
        namedWindow("7seg_binary", WINDOW_NORMAL);
        resizeWindow("7seg_binary", res.width, res.height);
    }

    auto fx = [](float k, int cell_w){ return max(1, int(k * cell_w + 0.5f)); };
    auto fy = [](float k, int cell_h){ return max(1, int(k * cell_h + 0.5f)); };

    while(true)
    {
        auto start = chrono::high_resolution_clock::now();

        if(!cap.read(frame))
            break;

        resize(frame, frame, res);
        cvtColor(frame, gray, COLOR_BGR2GRAY);
        gray.convertTo(gray, CV_32F, 1.0/255.0);

        if(invert_flag)
            gray = 1.0f - gray;

        frame_out = Scalar(0,0,0);

        for(size_t i = 0; i < cells.size(); i++){
            const Cell &c = cells[i];

            vector<Rect> SEG = {
                Rect(c.origin.x + fx(0.20f, c.w), c.origin.y + fy(0.05f, c.h), fx(0.60f, c.w), fy(0.12f, c.h)),
                Rect(c.origin.x + fx(0.78f, c.w), c.origin.y + fy(0.10f, c.h), fx(0.12f, c.w), fy(0.40f, c.h)),
                Rect(c.origin.x + fx(0.78f, c.w), c.origin.y + fy(0.50f, c.h), fx(0.12f, c.w), fy(0.40f, c.h)),
                Rect(c.origin.x + fx(0.20f, c.w), c.origin.y + fy(0.85f, c.h), fx(0.60f, c.w), fy(0.12f, c.h)),
                Rect(c.origin.x + fx(0.10f, c.w), c.origin.y + fy(0.50f, c.h), fx(0.12f, c.w), fy(0.40f, c.h)),
                Rect(c.origin.x + fx(0.10f, c.w), c.origin.y + fy(0.10f, c.h), fx(0.12f, c.w), fy(0.40f, c.h)),
                Rect(c.origin.x + fx(0.20f, c.w), c.origin.y + fy(0.45f, c.h), fx(0.60f, c.w), fy(0.12f, c.h))
            };

            for(int seg = 0; seg < 7; seg++){
                Rect roi = SEG[seg] & Rect(0,0,res.width,res.height);
                segStates[i].on[seg] = (roi.width > 0 && roi.height > 0 && mean(gray(roi))[0] >= thresh);
            }

            if(draw){
                for(int seg = 0; seg < 7; seg++){
                    if(segStates[i].on[seg])
                        rectangle(frame_out, SEG[seg], seg_color, FILLED);
                }
            }
        }

        if (draw) {
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
            Point origin(i * cell_w, j * cell_h);

            vector<Rect> SEG = {
                Rect(origin.x + int(0.20f * cell_w), origin.y + int(0.05f * cell_h), int(0.60f * cell_w), int(0.12f * cell_h)),
                Rect(origin.x + int(0.78f * cell_w), origin.y + int(0.10f * cell_h), int(0.12f * cell_w), int(0.40f * cell_h)),
                Rect(origin.x + int(0.78f * cell_w), origin.y + int(0.50f * cell_h), int(0.12f * cell_w), int(0.40f * cell_h)),
                Rect(origin.x + int(0.20f * cell_w), origin.y + int(0.85f * cell_h), int(0.60f * cell_w), int(0.12f * cell_h)),
                Rect(origin.x + int(0.10f * cell_w), origin.y + int(0.50f * cell_h), int(0.12f * cell_w), int(0.40f * cell_h)),
                Rect(origin.x + int(0.10f * cell_w), origin.y + int(0.10f * cell_h), int(0.12f * cell_w), int(0.40f * cell_h)),
                Rect(origin.x + int(0.20f * cell_w), origin.y + int(0.45f * cell_h), int(0.60f * cell_w), int(0.12f * cell_h))
            };

            for (int seg = 0; seg < 7; seg++) {
                if (b & (1 << seg)) {
                    rectangle(frame_out, SEG[seg], seg_color, FILLED);
                }
            }
        }
    }

    imshow("7seg_receive", frame_out);
    waitKey(1);
}
