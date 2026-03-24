#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <memory>

#include "video.h"
#include "audio.h"
#include "cli_parser.h"
#include "uart.h"
#include "framing.h"

using namespace std;
using namespace cv;

atomic<bool> stop_audio(false);

void run_send_mode(Params p, UART& uart) {
    vector<SegState> segStates;
    
    VideoCapture cap(p.filename);
    if(!cap.isOpened()) {
        cerr << "Error: Cannot open video: " << p.filename << endl;
        return;
    }

    if(p.max_fps <= 0) p.max_fps = cap.get(CAP_PROP_FPS);
    float frame_time = 1.0f / p.max_fps;

    Mat frame, gray, frame_out(p.res, CV_8UC3);

    auto prev = chrono::high_resolution_clock::now();
    int frame_count = 0;

    int nx = p.div.width;
    int ny = p.div.height;

    int cell_w = p.res.width / nx;
    int cell_h = p.res.height / ny;

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

    if (p.draw) {
        namedWindow("7seg_binary", WINDOW_NORMAL);
        resizeWindow("7seg_binary", p.res.width, p.res.height);
    }

    auto fx = [](float k, int cell_w){ return max(1, int(k * cell_w + 0.5f)); };
    auto fy = [](float k, int cell_h){ return max(1, int(k * cell_h + 0.5f)); };

    while(true)
    {
        auto start = chrono::high_resolution_clock::now();

        if(!cap.read(frame))
            break;

        resize(frame, frame, p.res);
        cvtColor(frame, gray, COLOR_BGR2GRAY);
        gray.convertTo(gray, CV_32F, 1.0/255.0);

        if(p.invert_flag)
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
                Rect roi = SEG[seg] & Rect(0,0,p.res.width,p.res.height);
                segStates[i].on[seg] = (roi.width > 0 && roi.height > 0 && mean(gray(roi))[0] >= p.thresh);
            }

            if(p.draw){
                for(int seg = 0; seg < 7; seg++){
                    if(segStates[i].on[seg])
                        rectangle(frame_out, SEG[seg], p.seg_color, FILLED);
                }
            }
        }

        Frame f = create_frame(segStates, nx, ny);
        vector<uint8_t> data = serialize(f);
        
        if (!uart.write(data)) {
            cerr << "UART write error" << endl;
            break;
        }

        if (p.draw) {
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
            cout << "\r\33[2KFPS: " << frame_count << " | Sent frames via UART" << flush;
            frame_count = 0;
            prev = now;
        }
    }
}

void run_receive_mode(const Params& p, UART& uart) {
    cout << "Receive mode: listening on " << p.port << " at " << p.baudrate << " baud" << endl;
    cout << "Press ESC to exit" << endl;

    namedWindow("7seg_receive", WINDOW_NORMAL);

    vector<uint8_t> buffer;
    const size_t expected_min_size = 5;
    const uint8_t frame_start_bytes[] = { 0x37, 0x21 };

    while(true)
    {
        auto data = uart.read(256, 100);
        if (data.empty()) {
            if (waitKey(1) == 27) break;
            continue;
        }

        buffer.insert(buffer.end(), data.begin(), data.end());

        while (buffer.size() >= expected_min_size) {
            auto it = search(buffer.begin(), buffer.end(), 
                            frame_start_bytes,
                            frame_start_bytes + 2);
            
            if (it == buffer.end()) {
                buffer.clear();
                break;
            }

            buffer.erase(buffer.begin(), it);

            if (buffer.size() < 5) break;

            uint8_t max_X = buffer[2];
            uint8_t max_Y = buffer[3];
            size_t frame_size = 5 + static_cast<size_t>(max_X) * max_Y;

            if (buffer.size() < frame_size) break;

            if (buffer[frame_size - 1] != FRAME_END) {
                buffer.erase(buffer.begin());
                continue;
            }

            try {
                vector<uint8_t> frame_data(buffer.begin(), buffer.begin() + frame_size);
                Frame f = deserialize(frame_data);
                
                display_frame(f, p.seg_color);
                
                buffer.erase(buffer.begin(), buffer.begin() + frame_size);
            } catch (const exception& e) {
                cerr << "Frame error: " << e.what() << endl;
                buffer.erase(buffer.begin());
            }
        }

        if (waitKey(1) == 27) break;
    }
}

void run_display_mode(const Params& p) {
    vector<SegState> segStates;
    run_video(p.filename, p.thresh, p.invert_flag, p.res, p.div, p.max_fps, p.draw, p.seg_color, segStates);
}

int main(int argc, char **argv)
{
    Params p = parse_args(argc, argv);

    if (p.mode == "send" || p.mode == "receive") {
        UART uart;
        if (!uart.open(p.port, p.baudrate)) {
            cerr << "Failed to open UART port: " << p.port << endl;
            return 1;
        }

        if (p.mode == "send") {
            if (p.audio_flag) {
                thread audio_thread(run_audio, p.filename, ref(stop_audio));
                run_send_mode(p, uart);
                stop_audio.store(true);
                if (audio_thread.joinable()) audio_thread.join();
            } else {
                run_send_mode(p, uart);
            }
        } else {
            run_receive_mode(p, uart);
        }

        uart.close();
    } else {
        if (p.audio_flag) {
            thread audio_thread(run_audio, p.filename, ref(stop_audio));
            run_display_mode(p);
            stop_audio.store(true);
            if (audio_thread.joinable()) audio_thread.join();
        } else {
            run_display_mode(p);
        }
    }

    return 0;
}