#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <cstdlib>

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

    Size eff_res = effective_resolution(p.res, p.rotation);

    Mat frame, gray, frame_out(eff_res, CV_8UC3);

    auto prev = chrono::high_resolution_clock::now();
    int frame_count = 0;

    vector<Cell> cells = build_cells(eff_res, p.div);
    segStates.resize(cells.size());

    if (p.draw) {
        namedWindow("7seg_binary", WINDOW_NORMAL | WINDOW_GUI_NORMAL);
        resizeWindow("7seg_binary", eff_res.width, eff_res.height);
    }

    while(true){
        auto start = chrono::high_resolution_clock::now();

        if(!cap.read(frame))
            break;

        prepare_frame(frame, gray, eff_res, p.rotation, p.invert_flag);

        process_frame_to_segstates(gray, cells, p.thresh, segStates);

        if(p.draw){
            frame_out = Scalar(0,0,0);
            draw_segstates(frame_out, cells, segStates, p.seg_color);
        }

        Frame f = create_frame(segStates,
                               static_cast<uint8_t>(p.div.width),
                               static_cast<uint8_t>(p.div.height));
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

void run_receive_mode(const Params& p, UART& uart){
    cout << "Receive mode: listening on " << p.port << " at " << p.baudrate << " baud" << endl;

    bool display_available = false;
    if (p.draw) {
        display_available = true;
        cout << "Display enabled - press ESC to exit" << endl;
    }

    vector<uint8_t> buffer;
    const size_t expected_min_size = 5;
    const uint8_t frame_start_bytes[] = { 0x37, 0x21 };

    while(true)
    {
        auto data = uart.read(256, 100);
        if (data.empty()) {
            if (display_available && waitKey(1) == 27) break;
            if (!display_available) this_thread::sleep_for(chrono::milliseconds(100));
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
                
                if (display_available) {
                    display_frame(f, p.seg_color);
                } else {
                    static int frame_count = 0;
                    frame_count++;
                    if (frame_count % 30 == 0) {
                        cout << "Received frame " << frame_count 
                             << " (X=" << static_cast<int>(f.max_X) 
                             << ", Y=" << static_cast<int>(f.max_Y) 
                             << ", " << f.cells.size() << " bytes)" << endl;
                    }
                }
                
                buffer.erase(buffer.begin(), buffer.begin() + frame_size);
            } catch (const exception& e) {
                cerr << "Frame error: " << e.what() << endl;
                buffer.erase(buffer.begin());
            }
        }

        if (display_available && waitKey(1) == 27) break;
    }
}

void run_display_mode(const Params& p){
    vector<SegState> segStates;
    run_video(p.filename, p.thresh, p.invert_flag, p.res, p.div, p.max_fps, p.rotation, p.draw, p.seg_color, segStates);
}

int main(int argc, char **argv){
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
