#include "video.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

using namespace cv;
using namespace std;

struct SegState{
    bool on[7]; // A,B,C,D,E,F,G states
};

void run_video(const string &filename,
               float thresh,
               bool invert_flag,
               Size res,    // resolution of video frame
               Size div,    // number of displays (cols, rows)
               float max_fps,
               bool draw,
	       Scalar seg_color)
{
    VideoCapture cap(filename);
    if(!cap.isOpened()) {
        cerr << "Error: Cannot open video: " << filename << endl;
        return;
    }

    float frame_time = 1.0f / max_fps;

    Mat frame, gray, frame_out(res, CV_8UC3);

    auto prev = chrono::high_resolution_clock::now();
    int frame_count = 0;

    // --- Compute perfect grid ---
    int nx = div.width;   // number of columns
    int ny = div.height;  // number of rows

    int cell_w = res.width / nx;
    int cell_h = res.height / ny;

    // Generate top-left corner of each cell
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

    vector<SegState> segStates(cells.size());

    namedWindow("7seg_binary", WINDOW_NORMAL);
    resizeWindow("7seg_binary", res.width, res.height);

    // --- Segment scaling helpers ---
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

            // --- Define 7-segment rectangles for this cell ---
            vector<Rect> SEG = {
                Rect(c.origin.x + fx(0.20f, c.w), c.origin.y + fy(0.05f, c.h), fx(0.60f, c.w), fy(0.12f, c.h)), // A
                Rect(c.origin.x + fx(0.78f, c.w), c.origin.y + fy(0.10f, c.h), fx(0.12f, c.w), fy(0.40f, c.h)), // B
                Rect(c.origin.x + fx(0.78f, c.w), c.origin.y + fy(0.50f, c.h), fx(0.12f, c.w), fy(0.40f, c.h)), // C
                Rect(c.origin.x + fx(0.20f, c.w), c.origin.y + fy(0.85f, c.h), fx(0.60f, c.w), fy(0.12f, c.h)), // D
                Rect(c.origin.x + fx(0.10f, c.w), c.origin.y + fy(0.50f, c.h), fx(0.12f, c.w), fy(0.40f, c.h)), // E
                Rect(c.origin.x + fx(0.10f, c.w), c.origin.y + fy(0.10f, c.h), fx(0.12f, c.w), fy(0.40f, c.h)), // F
                Rect(c.origin.x + fx(0.20f, c.w), c.origin.y + fy(0.45f, c.h), fx(0.60f, c.w), fy(0.12f, c.h))  // G
            };

            // --- Compute segment states ---
            for(int seg = 0; seg < 7; seg++){
                Rect roi = SEG[seg] & Rect(0,0,res.width,res.height);
                segStates[i].on[seg] = (roi.width > 0 && roi.height > 0 && mean(gray(roi))[0] >= thresh);
            }

            // --- Draw segments ---
            if(draw){
                for(int seg = 0; seg < 7; seg++){
                    if(segStates[i].on[seg])
                        rectangle(frame_out, SEG[seg], seg_color, FILLED);
                }
            }
        }

        imshow("7seg_binary", frame_out);

        if(waitKey(1) == 27)
            break;

        // --- FPS limiter ---
        float elapsed = chrono::duration<float>(chrono::high_resolution_clock::now() - start).count();
        if(elapsed < frame_time)
            this_thread::sleep_for(chrono::duration<float>(frame_time - elapsed));

        // --- FPS counter ---
        frame_count++;
        auto now = chrono::high_resolution_clock::now();
        if(chrono::duration<float>(now - prev).count() >= 1.0f){
            cout << "\r\33[2KFPS: " << frame_count << flush;
            frame_count = 0;
            prev = now;
        }
    }
}

