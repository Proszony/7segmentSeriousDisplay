#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include "video.h"
#include "audio.h"

using namespace std;
using namespace cv;


atomic<bool> stop_audio(false);

const char *help = R"(
Simulation help.
Flags:
-inv    : invert gray scale video
-th     : set threshold
-fps    : set fps cap
-d	: show simulation in CLI
-a	: play audio
-h      : show this message
-i      : input video file
)";

int main(int argc, char **argv)
{
    string filename = "BadApple!!.mp4";
    float thresh = 0.5f;
    bool invert_flag = false;
    bool audio_flag = false;
    Size res(640, 380);
    Size div(20, 32);
    float max_fps = 30.0f;
    bool draw = false;

    // --------------------------
    // Parse command-line flags
    // --------------------------
    for(int i = 1; i < argc; i++) {
        string arg = argv[i];

        if(arg == "-i" && i+1 < argc)
            filename = argv[++i];
        else if(arg == "-h"){
            cout << help << endl;
            return 0;
        }
        else if(arg == "-th" && i+1 < argc)
            thresh = stof(argv[++i]);
        else if(arg == "-inv")
            invert_flag = true;
        else if(arg == "-fps" && i+1 < argc)
            max_fps = stof(argv[++i]);
        else if(arg == "-a")
            audio_flag = true;
	else if(arg == "-d")
	    draw = true;	
        else
            cout << "Unknown flag: " << arg << endl;
    }

    // --------------------------
    // Run audio in a separate thread (if enabled)
    // --------------------------
    std::thread audio_thread;
    if(audio_flag)
        audio_thread = std::thread(run_audio, filename, std::ref(stop_audio));

    // --------------------------
    // Run video on main thread
    // --------------------------
    run_video(filename, thresh, invert_flag, res, div, max_fps, draw);
    stop_audio.store(true);

    // --------------------------
    // Wait for audio to finish
    // --------------------------
    if(audio_flag && audio_thread.joinable())
        audio_thread.join();

    return 0;
}

