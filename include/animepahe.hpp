#pragma once

#ifndef ANIMEPAHE_HPP
#define ANIMEPAHE_HPP

#include <cpr/cpr.h>
#include <map>
#include <vector>
#include <string>

namespace AnimepaheCLI
{
    class Animepahe
    {
    private:
        cpr::Header getHeaders(const std::string &link);
        std::map<std::string, std::string> fetch_episode(const std::string &link, const int &targetRes, const std::string &audioLang);
        int get_series_episode_count(const std::string& link);
        std::vector<std::string> fetch_series(const std::string &link, const int epCount, bool isAllEpisodes, const std::vector<int> &episodes);
        std::string extract_link_metadata(const std::string &link, bool isSeries);
        std::vector<std::map<std::string, std::string>> extract_link_content(
            const std::string &link,
            const std::vector<int> &episodes,
            const int targetRes,
            const std::string &audioLang,
            bool isSeries,
            bool isAllEpisodes
        );
    public:
        void extractor(
            bool isSeries,
            const std::string &link,
            const int targetRes,
            const std::string &audioLang,
            bool isAllEpisodes,
            const std::vector<int> &episodes,
            const std::string &export_filename,
            bool exportLinks = false,
            bool createZip = false,
            bool removeSource = false
        );
    };
}

#endif
