#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>

#include "video.h"
#include "audio.h"
#include "cli_parser.h"


using namespace std;
using namespace cv;


atomic<bool> stop_audio(false);

int main(int argc, char **argv)
{
    Params p = parse_args(argc, argv);

    // --------------------------
    // Run audio in a separate thread (if enabled)
    // --------------------------
    std::thread audio_thread;
    if(p.audio_flag)
        audio_thread = std::thread(run_audio, p.filename, std::ref(stop_audio));

    // --------------------------
    // Run video on main thread
    // --------------------------
    run_video(p.filename, p.thresh, p.invert_flag, p.res, p.div, p.max_fps, p.draw, p.seg_color);
    stop_audio.store(true);

    // --------------------------
    // Wait for audio to finish
    // --------------------------
    if(p.audio_flag && audio_thread.joinable())
        audio_thread.join();

    return 0;
}

