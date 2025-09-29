#pragma once
#include <string>
bool stream_with_ffmpeg(const std::string &url, const std::string &audio_lang);
bool preview_with_ffmpeg(const std::string &url, int duration_seconds, const std::string &temp_file);
