#include "cli_parser.h"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <unordered_map>
#include <functional>

static const char* help_text = R"(
Simulation help.
Flags:
  -inv, 	--invert		: invert gray scale video
  -th <value>	--threshold <value>	: set threshold
  -fps <value>	--fpscap <value>	: set fps cap
  -res <w,h>	--resolution <w,h>	: set display resolution
  -div <nx,ny>	--dividents <nx,ny>	: set number of segments per axis
  -c <r,g,b>	--color <r,g,b>		: set 7 segment display color
  -d      	--draw			: show simulation
  -a      	--audio			: play audio
  -i <file>	--input <file>		: input video file (.mp4)
  -h		--help			: show this message

Defaults:
  file(from res directory)	: BadApple!!.mp4
  threshold   			: 0.5
  invert      			: false
  audio       			: false
  video       			: false
  fps         			: 30
  color				: (255,0,0)
  res				: (640, 360)
  div				: (32, 12)
)";

void print_help() {
    std::cout << help_text << std::endl;
}

cv::Size parse_size(const std::string &arg) {
    int a=0, b=0;
    char comma;
    std::istringstream ss(arg);
    ss >> a >> comma >> b;
    if(ss.fail() || comma != ',') {
        std::cerr << "Failed to parse size from: " << arg << std::endl;
        return cv::Size(0,0);
    }
    return cv::Size(a,b);
}

cv::Scalar parse_color(const std::string &arg){
    int r=255, g=0, b=0;
    char comma1, comma2;
    std::istringstream ss(arg);
    ss >> r >> comma1 >> g >> comma2 >> b;
    if(ss.fail() || comma1 != ',' || comma2 != ','){
        std::cerr << "Failed to parse color from: " << arg << std::endl;
        return cv::Scalar(0,0,255); // default red
    }
    return cv::Scalar(b,g,r); // return BGR for OpenCV
}

Params parse_args(int argc, char** argv)
{
    Params p;

    // Map of all short and long flags to their handler lambdas
    std::unordered_map<std::string, std::function<void(int&, char**)>> handlers = {
        {"-i", [&](int &i, char** argv){ if(i+1<argc) p.filename = argv[++i]; }},
        {"--input", [&](int &i, char** argv){ if(i+1<argc) p.filename = argv[++i]; }},

        {"-th", [&](int &i, char** argv){ if(i+1<argc) p.thresh = std::stof(argv[++i]); }},
        {"--threshold", [&](int &i, char** argv){ if(i+1<argc) p.thresh = std::stof(argv[++i]); }},

        {"-fps", [&](int &i, char** argv){ if(i+1<argc) p.max_fps = std::stof(argv[++i]); }},
        {"--fpscap", [&](int &i, char** argv){ if(i+1<argc) p.max_fps = std::stof(argv[++i]); }},

        {"-c", [&](int &i, char** argv){ if(i+1<argc) p.seg_color = parse_color(argv[++i]); }},
        {"--color", [&](int &i, char** argv){ if(i+1<argc) p.seg_color = parse_color(argv[++i]); }},

        {"-res", [&](int &i, char** argv){ if(i+1<argc) p.res = parse_size(argv[++i]); }},
        {"--resolution", [&](int &i, char** argv){ if(i+1<argc) p.res = parse_size(argv[++i]); }},

        {"-div", [&](int &i, char** argv){ if(i+1<argc) p.div = parse_size(argv[++i]); }},
        {"--dividents", [&](int &i, char** argv){ if(i+1<argc) p.div = parse_size(argv[++i]); }},

        {"-inv", [&](int &i, char**){ p.invert_flag = true; }},
        {"--invert", [&](int &i, char**){ p.invert_flag = true; }},

        {"-a", [&](int &i, char**){ p.audio_flag = true; }},
        {"--audio", [&](int &i, char**){ p.audio_flag = true; }},

        {"-d", [&](int &i, char**){ p.draw = true; }},
        {"--draw", [&](int &i, char**){ p.draw = true; }},

        {"-h", [&](int &i, char**){ print_help(); std::exit(0); }},
        {"--help", [&](int &i, char**){ print_help(); std::exit(0); }}
    };

    for(int i = 1; i < argc; i++){
        std::string arg = argv[i];
        auto it = handlers.find(arg);
        if(it != handlers.end()){
            it->second(i, argv); // call the lambda
        } else {
            std::cout << "Unknown flag: " << arg << std::endl;
        }
    }

    return p;
}
