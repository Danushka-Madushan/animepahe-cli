#pragma once

#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>
#include <map>
#include <sstream>

namespace AnimepaheCLI
{
    std::string replaceSpacesWithUnderscore(std::string text);
    std::string sanitizeForWindowsPath(std::string name);
    int getPage(int number);
    bool isValidTxtFilename(const std::string& filename);
    std::vector<int> getPaginationRange(int start, int end);
    std::string sanitize_utf8(const std::string &input);
    bool isFullSeriesURL(const std::string &url);
    bool isEpisodeURL(const std::string &url);
    bool isValidEpisodeRangeFormat(const std::string &input);
    std::vector<int> parseEpisodeRange(const std::string &input);
    std::string unescape_html_entities(const std::string &input);
    std::string padIntWithZero(int num);
    std::string base64_encode(const std::string &in);
    
    template <typename T>
    std::string vectorToString(const std::vector<T> &vec)
    {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < vec.size(); ++i)
        {
            oss << "EP" << padIntWithZero(vec[i]);
            if (i != vec.size() - 1)
                oss << ", ";
        }
        oss << "]";
        return oss.str();
    }
}

#endif
