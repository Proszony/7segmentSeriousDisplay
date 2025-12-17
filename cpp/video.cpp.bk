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
               Size res,
               Size div,
               float max_fps,
	       bool draw)
{
    VideoCapture cap(filename);
    if(!cap.isOpened()) {
        cerr << "Error: Cannot open video: " << filename << endl;
        return;
    }

    // Prepare grid cell origins
    vector<Point> boxes;
    for(int x = 0; x < res.width;  x += div.width)
        for(int y = 0; y < res.height; y += div.height)
            boxes.emplace_back(x, y);

    float frame_time = 1.0f / max_fps;

    Mat frame, gray, frame_out(res, CV_8UC3);

    // Segment layout
    struct Seg { int ox, oy, w, h; };
    auto fx = [&](float k){ return int(k * div.width);  };
    auto fy = [&](float k){ return int(k * div.height); };

    vector<Seg> SEG = {
        { fx(0.20f), fy(0.05f), fx(0.60f), fy(0.12f) },   // A
        { fx(0.78f), fy(0.10f), fx(0.12f), fy(0.40f) },   // B
        { fx(0.78f), fy(0.50f), fx(0.12f), fy(0.40f) },   // C
        { fx(0.20f), fy(0.85f), fx(0.60f), fy(0.12f) },   // D
        { fx(0.10f), fy(0.50f), fx(0.12f), fy(0.40f) },   // E
        { fx(0.10f), fy(0.10f), fx(0.12f), fy(0.40f) },   // F
        { fx(0.20f), fy(0.45f), fx(0.60f), fy(0.12f) }    // G
    };

    auto prev = chrono::high_resolution_clock::now();
    int frame_count = 0;
   
    vector<SegState> segStates(boxes.size());
    
    namedWindow("7seg_binary", WINDOW_NORMAL);
    resizeWindow("7seg_binary", res.width, res.height);

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

	for(size_t i = 0; i < boxes.size(); i++){
		const Point &p = boxes[i];
	
		// State computation	
		for(int seg = 0; seg < 7; seg++){
			const auto &s = SEG[seg];

			Rect roi(
				p.x + s.ox,
				p.y + s.oy,
				s.w,
				s.h
			);

			roi &= Rect(0,0,res.width,res.height);
			if (roi.width < 1 || roi.height < 1) {
				segStates[i].on[seg] = false;
				continue;
			}

			float meanVal = mean(gray(roi))[0];

			segStates[i].on[seg] = (meanVal >= thresh);
		}
		
		// Drawing segments
		if(draw){
			for(int seg = 0; seg < 7; seg++){
				if(!segStates[i].on[seg]) continue;

				const auto &s = SEG[seg];

				Rect roi(
					p.x + s.ox,
					p.y + s.oy,
					s.w,
					s.h			
				);

				rectangle(frame_out, roi, Scalar(0, 0, 255), FILLED);
			}
		}
	}

        imshow("7seg_binary", frame_out);

        if(waitKey(1) == 27)
            break;

        // FPS limiter
        float elapsed = chrono::duration<float>(
            chrono::high_resolution_clock::now() - start
        ).count();

        if(elapsed < frame_time)
            this_thread::sleep_for(chrono::duration<float>(frame_time - elapsed));

        // FPS counter
        frame_count++;
        auto now = chrono::high_resolution_clock::now();
        if(chrono::duration<float>(now - prev).count() >= 1.0f){
            cout << "\r\33[2KFPS: " << frame_count << flush;
            frame_count = 0;
            prev = now;
        }
    }
}

