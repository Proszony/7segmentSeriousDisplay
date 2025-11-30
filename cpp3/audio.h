#pragma once
#include <string>
#include <atomic>

void run_audio(const std::string &filename, std::atomic<bool> &stop_flag);

