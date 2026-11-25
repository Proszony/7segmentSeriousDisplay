#include "cli_parser.h"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <unordered_map>
#include <functional>
#include <optional>

static const char* help_text = R"(
UART 7-Seg Dispaly help.
Flags:
  -m, --mode <mode>       		: set mode (send|receive|display) [default: send]
  -p, --port <path>     		: UART port path [default: /dev/ttyUSB0]
  -b, --baud <value>     		: UART baudrate [default: 115200]
  -inv,  --invert         		: invert gray scale video
  -th <value>  --threshold <value>	: set threshold
  -fps <value>	--fpscap <value>	: set fps cap (-1 for automatic fps)
  -res <w,h>	--resolution <w,h>	: set display resolution
  -div <nx,ny>	--dividents <nx,ny>	: set number of segments per axis
  -rot <deg>	--rotate <deg>		: rotate video (0|90|180|270)
  -c <r,g,b>	--color <r,g,b>		: set 7 segment display color
  -d      	--draw			: show simulation
  -a      	--audio			: play audio
  -i <file>	--input <file>		: input video file (.mp4)
  -h		--help			: show this message

Defaults:
  mode			: send
  port			: /dev/ttyUSB0
  baudrate		: 115200
  file(from res dir)	: BadApple!!.mp4
  threshold		: 0.5
  fps			: 30
  color			: (255,0,0)
  res			: (640, 360)
  div			: (32, 24)
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
        return cv::Scalar(0,0,255);
    }
    return cv::Scalar(b,g,r);
}

std::optional<std::string> validate_params(const Params& p){

	if(!std::isfinite(p.thresh) || p.thresh < 0.0f || p.thresh > 1.0f){
		return "Threshold must be finite and in range of [0,1]";
	}
	if(!std::isfinite(p.max_fps) || p.max_fps == 0.0f){
		return "Fps must be finite and not a zero";
	}
	if(p.res.width <= 0 || p.res.height <= 0){
		return "Resolution must be positive";
	}
	if(p.mode != "send" && p.mode !=  "receive" && p.mode != "display"){
		return "Unknown mode";
	} 
	if(p.div.width < 1 || p.div.height < 1){
		
		return "Invalid number of segments. Number must be positive"; 
	}else {
		if(p.mode != "display" && (p.div.height > 255 || p.div.width > 255)){
			return "Invalid number of segments. Div must be between [1,255] for send and receive modes";
		}
	}
	
	const cv::Size eff_res = (p.rotation == 90 || p.rotation == 270) ? cv::Size(p.res.height, p.res.width) : p.res;

	if(p.div.width > eff_res.width || p.div.height > eff_res.height){
		return "Number of segments exceeds resolution";
	}

    return std::nullopt;
}


Params parse_args(int argc, char** argv){
    Params p;

    std::unordered_map<std::string, std::function<void(int&, char**)>> handlers = {
        {"-m", [&](int &i, char** argv){ if(i+1<argc) p.mode = argv[++i]; }},
        {"--mode", [&](int &i, char** argv){ if(i+1<argc) p.mode = argv[++i]; }},

        {"-p", [&](int &i, char** argv){ if(i+1<argc) p.port = argv[++i]; }},
        {"--port", [&](int &i, char** argv){ if(i+1<argc) p.port = argv[++i]; }},

        {"-b", [&](int &i, char** argv){ if(i+1<argc) p.baudrate = static_cast<unsigned int>(std::stoul(argv[++i])); }},
        {"--baud", [&](int &i, char** argv){ if(i+1<argc) p.baudrate = static_cast<unsigned int>(std::stoul(argv[++i])); }},

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

        {"-rot", [&](int &i, char** argv){ if(i+1<argc) p.rotation = std::stoi(argv[++i]); }},
        {"--rotate", [&](int &i, char** argv){ if(i+1<argc) p.rotation = std::stoi(argv[++i]); }},

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
            it->second(i, argv);
        } else {
            std::cout << "Unknown flag: " << arg << std::endl;
        }
    }

    if(p.rotation != 0 && p.rotation != 90 && p.rotation != 180 && p.rotation != 270){
        std::cerr << "Invalid rotation: " << p.rotation << " (allowed: 0|90|180|270)" << std::endl;
        std::exit(1);
    }

    if(auto err = validate_params(p)){
    	std::cerr << *err << std::endl;
	std::exit(1);
    }

    return p;

}
