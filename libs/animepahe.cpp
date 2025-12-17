#include <animepahe.hpp>
#include <kwikpahe.hpp>
#include <downloader.hpp>
#include <cpr/cpr.h>
#include <re2/re2.h>
#include <fmt/core.h>
#include <fmt/color.h>
#include <utils.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <ziputils.hpp>
#include <iostream>

using json = nlohmann::json;

namespace AnimepaheCLI
{
    cpr::Cookies cookies = cpr::Cookies{{"__ddg2_", ""}};
    const char *CLEAR_LINE = "\033[2K"; // Clear entire line
    const char *MOVE_UP = "\033[1A";    // Move cursor up 1 line
    const char *CURSOR_START = "\r";    // Return to start of line

    /* Extract Kwik from pahe.win */
    KwikPahe kwikpahe;

    cpr::Header Animepahe::getHeaders(const std::string &link)
    {
        const cpr::Header HEADERS = {
            {"accept", "application/json, text/javascript, */*; q=0.0"},
            {"accept-language", "en-US,en;q=0.9"},
            {"referer", link},
            {"user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/138.0.0.0 Safari/537.36 Edg/138.0.0.0"}};
        return HEADERS;
    }

    std::string Animepahe::extract_link_metadata(const std::string &link, bool isSeries)
    {
        fmt::print("\n\r * Requesting Info..");
        cpr::Response response = cpr::Get(
            cpr::Url{link},
            cpr::Header{getHeaders(link)}, cookies);

        fmt::print("\r * Requesting Info : ");

        /* series_name */
        std::string series_title;

        if (response.status_code != 200)
        {
            fmt::print(fmt::fg(fmt::color::indian_red), "FAILED!\n");
            throw std::runtime_error(fmt::format("Failed to fetch {}, StatusCode: {}", link, response.status_code));
        }
        else
        {
            fmt::print(fmt::fg(fmt::color::lime_green), "OK!\n");
        }

        RE2::GlobalReplace(&response.text, R"((\r\n|\r|\n))", "");

        if (isSeries)
        {
            re2::StringPiece TITLE_CONSUME = response.text;
            re2::StringPiece TYPE_CONSUME = response.text;
            re2::StringPiece EP_CONSUME = response.text;
            std::string title;
            std::string type;
            std::string episodesCount;

            if (RE2::FindAndConsume(&TITLE_CONSUME, R"re(style=[^=]+title="([^"]+)")re", &title))
            {
                title = unescape_html_entities(title);
                series_title = title;
            }

            if (RE2::FindAndConsume(&TYPE_CONSUME, R"re(Type:[^>]*title="[^"]*"[^>]*>([^<]+)</a>)re", &type))
            {
                type = unescape_html_entities(type);
            }

            if (RE2::FindAndConsume(&TYPE_CONSUME, R"re(Episode[^>]*>\s*(\S*)</p)re", &episodesCount))
            {
                episodesCount = unescape_html_entities(episodesCount);
            }

            fmt::print("\n * Anime: {}\n", title);
            fmt::print(" * Type: {}\n", type);
            fmt::print(" * Episodes: {}\n", episodesCount);
        }
        else
        {
            re2::StringPiece TITLE_TYPE_CONSUME = response.text;
            std::string title;
            std::string episode;

            if (RE2::FindAndConsume(&TITLE_TYPE_CONSUME, R"re(title="[^>]*>([^<]*)</a>\D*(\d*)<span)re", &title, &episode))
            {
                episode = unescape_html_entities(episode);
                title = unescape_html_entities(title);
                series_title = title;
            }

            fmt::print("\n * Anime: {}\n", title);
            fmt::print(" * Episode: {}\n", episode);
        }
        /* return series_name */
        return series_title;
    }

    std::map<std::string, std::string> Animepahe::fetch_episode(const std::string &link, const int &targetRes, const std::string &audioLang)
    {
        std::vector<std::map<std::string, std::string>> episodeData;
        cpr::Response response = cpr::Get(
            cpr::Url{link},
            cpr::Header{getHeaders(link)}, cookies);

        if (response.status_code != 200)
        {
            fmt::print("\n * Error: Failed to fetch {}, StatusCode {}\n", link, response.status_code);
            return {};
        }

        RE2::GlobalReplace(&response.text, R"((\r\n|\r|\n))", "");
        re2::StringPiece EP_CONSUME = response.text;
        std::string dPaheLink;
        std::string epBlock;

        while (RE2::FindAndConsume(&EP_CONSUME, R"re(<a href="(https://pahe\.win/\S*)"[^>]*>(.*?)</a>)re", &dPaheLink, &epBlock))
        {
            std::map<std::string, std::string> content;
            content["dPaheLink"] = unescape_html_entities(dPaheLink);
            re2::StringPiece epPiece(epBlock);
            std::string sourceText;
            RE2::PartialMatch(epPiece, R"re(\s*([^<]*))re", &sourceText);
            sourceText.erase(sourceText.find_last_not_of(" \t\n\r") + 1);
            content["sourceText"] = unescape_html_entities(sourceText);

            epPiece = re2::StringPiece(epBlock);
            std::string epRes;
            if (RE2::FindAndConsume(&epPiece, R"re(\b(\d{3,4})p\b)re", &epRes))
            {
                epRes = unescape_html_entities(epRes);
            }
            else
            {
                epRes = "0"; // default
            }
            content["epRes"] = epRes;
            
            content["epLang"] = "jp"; // default if not changed later.

            // Check for a span containing the language.
            epPiece = re2::StringPiece(epBlock);
            std::string spanContent;
            while (RE2::FindAndConsume(&epPiece, R"re(<span[^>]*>([^<]*)</span>)re", &spanContent)) {
                std::transform(spanContent.begin(), spanContent.end(), spanContent.begin(), ::tolower);
                if (spanContent == "bd")
                {
                    // Do nothing so far with the Blu-ray disk tag.
                }
                else if (spanContent == "dub")
                {
                    content["epLang"] = "eng";
                    break;
                }
                else if (spanContent == "chi")
                {
                    content["epLang"] = "zh";
                    break;
                }
                else
                {
                    content["epLang"] = spanContent; // treat remaining spans as a language.
                    break;
                }
            }

            episodeData.push_back(content);
        }

        if (episodeData.empty())
        {
            throw std::runtime_error(fmt::format("\n No episodes found in {}", link));
        }

        /**
         * Filter episodes by language preference
         * AnimePahe sorts JPN episodes at top, followed by ENG
         * We filter by checking if episode name contains language indicators
         */
        std::vector<std::map<std::string, std::string>> filteredData;
        for (const auto& episode : episodeData)
        {
            const std::string& epLang = episode.at("epLang");

            if (
                (audioLang == "en" && epLang == "eng") ||
                (audioLang == "jp" && epLang == "jp")  ||
                (audioLang == "zh" && epLang == "zh")
            ) {
                filteredData.push_back(episode);
            }
        }

        // Fallback to all episodes if no matches found for requested language
        if (filteredData.empty())
        {
            filteredData = episodeData;
        }

        /**
         * check if there is a provided resolution
         * if there is a match then return it otherwise Find the episode with the highest resolution
         */
        std::map<std::string, std::string>* selectedEpMap = nullptr;
        std::map<std::string, std::string>* maxEpMap = nullptr;
        std::map<std::string, std::string>* minEpMap = nullptr;

        int maxEpRes = 0;
        int minEpRes = INT_MAX; /* use INT_MAX to ensure proper min comparison */

        bool selectHighestQuality = (targetRes == 0);
        bool selectLowestQuality  = (targetRes == -1);
        bool isCustomQualityProvided = (targetRes > 0); /* only valid custom inputs are > 0 */

        for (auto& episode : filteredData)
        {
            int epResValue = std::stoi(episode.at("epRes"));

            /* Track highest */
            if (epResValue > maxEpRes)
            {
                maxEpRes = epResValue;
                maxEpMap = &episode;
            }

            /* Track lowest */
            if (epResValue < minEpRes)
            {
                minEpRes = epResValue;
                minEpMap = &episode;
            }

            /* If custom quality requested */
            if (isCustomQualityProvided && epResValue == targetRes)
            {
                selectedEpMap = &episode;
                break; /* exact match found, no need to continue */
            }
        }

        /* Final decision */
        if (selectedEpMap == nullptr)
        {
            if (selectHighestQuality) {
                selectedEpMap = maxEpMap;
            } else if (selectLowestQuality) {
                selectedEpMap = minEpMap;
            }
            /* else (custom and not found) fallback to max */
            else {
                selectedEpMap = maxEpMap;
            }
        }

        return *selectedEpMap;
    }

    std::vector<std::string> Animepahe::fetch_series(
        const std::string &link,
        const int epCount,
        bool isAllEpisodes,
        const std::vector<int> &episodes)
    {
        std::vector<std::string> links;
        std::vector<int> paginationPages;

        if (isAllEpisodes)
        {
            paginationPages = getPaginationRange(1, epCount);
        }
        else
        {
            paginationPages = getPaginationRange(episodes[0], episodes[1]);
        }

        std::string id;
        RE2::PartialMatch(link, R"(anime/([a-f0-9-]{36}))", &id);
        fmt::print("\n\r * Requesting Pages..");
        for (auto &page : paginationPages)
        {
            fmt::print("\r * Requesting Pages : {}", page);
            fflush(stdout);
            cpr::Response response = cpr::Get(
                cpr::Url{
                    fmt::format("https://animepahe.si/api?m=release&id={}&sort=episode_asc&page={}", id, page)},
                cpr::Header{getHeaders(link)}, cookies);

            if (response.status_code != 200)
            {
                throw std::runtime_error(fmt::format("\n * Error: Failed to fetch {}, StatusCode {}\n", link, response.status_code));
            }

            auto parsed = json::parse(response.text);

            if (parsed.contains("data") && parsed["data"].is_array())
            {
                for (const auto &episode : parsed["data"])
                {
                    std::string session = episode.value("session", "unknown");
                    std::string episodeLink = fmt::format("https://animepahe.si/play/{}/{}", id, session);
                    links.push_back(episodeLink);
                }
            }
        }
        fmt::print("\r * Requesting Pages :");
        fmt::print(fmt::fg(fmt::color::lime_green), " OK!\n\r");

        return links;
    }

    int Animepahe::get_series_episode_count(const std::string &link)
    {
        std::string id;
        RE2::PartialMatch(link, R"(anime/([a-f0-9-]{36}))", &id);

        cpr::Response response = cpr::Get(
            cpr::Url{
                fmt::format("https://animepahe.si/api?m=release&id={}&sort=episode_asc&page={}", id, 1)},
            cpr::Header{getHeaders(link)}, cookies);

        if (response.status_code != 200)
        {
            throw std::runtime_error(fmt::format("\n * Error: Failed to fetch {}, StatusCode {}\n", link, response.status_code));
        }

        auto parsed = json::parse(response.text);
        int epCount = 0;
        if (parsed.contains("total") && parsed["total"].is_number_integer())
        {
            epCount = parsed["total"];
        }

        return epCount;
    }

    std::vector<std::map<std::string, std::string>> Animepahe::extract_link_content(
        const std::string &link,
        const std::vector<int> &episodes,
        const int targetRes,
        const std::string &audioLang,
        bool isSeries,
        bool isAllEpisodes)
    {
        std::vector<std::map<std::string, std::string>> episodeListData;

        if (isSeries)
        {
            const int epCount = get_series_episode_count(link);
            std::vector<std::string> seriesEpLinks = fetch_series(link, epCount, isAllEpisodes, episodes);

            if (isAllEpisodes)
            {
                for (int i = 0; i < seriesEpLinks.size(); ++i)
                {
                    const std::string &pLink = seriesEpLinks[i];
                    fmt::print("\r * Requesting Episode : EP{} ", padIntWithZero(i + 1));
                    std::map<std::string, std::string> epContent = fetch_episode(pLink, targetRes, audioLang);
                    fflush(stdout);
                    if (!epContent.empty())
                    {
                        episodeListData.push_back(epContent);
                    }
                }
            }
            else
            {
                if (episodes[0] > epCount || episodes[1] > epCount)
                {
                    throw std::runtime_error(fmt::format("Invalid episode range: {}-{} for series with {} episodes", episodes[0], episodes[1], epCount));
                }

                std::vector<int> paginationPages = getPaginationRange(episodes[0], episodes[1]);
                int offset = paginationPages[0] == 1 ? 0 : (30 * (paginationPages[0] - 1));

                for (int i = offset; i < (seriesEpLinks.size() + offset); ++i)
                {
                    const std::string &pLink = seriesEpLinks[i - offset];

                    if ((i >= episodes[0] - 1 && i <= episodes[1] - 1))
                    {
                        fmt::print("\r * Requesting Episode : EP{} ", padIntWithZero(i + 1));
                        std::map<std::string, std::string> epContent = fetch_episode(pLink, targetRes, audioLang);
                        fflush(stdout);
                        if (!epContent.empty())
                        {
                            episodeListData.push_back(epContent);
                        }
                    }
                }
            }
        }
        else
        {

            std::map<std::string, std::string> epContent = fetch_episode(link, targetRes, audioLang);
            if (epContent.empty())
            {
                fmt::print("\n * Error: No episode data found for {}\n", link);
                return {};
            }

            episodeListData.push_back(epContent);
        }

        fmt::print("\r * Requesting Episodes : {} ", episodeListData.size());
        fmt::print(fmt::fg(fmt::color::lime_green), "OK!\n");
        return episodeListData;
    }

    void Animepahe::extractor(
        bool isSeries,
        const std::string &link,
        const int targetRes,
        const std::string &audioLang,
        bool isAllEpisodes,
        const std::vector<int> &episodes,
        const std::string &export_filename,
        bool exportLinks,
        bool createZip,
        bool removeSource
    )
    {
        /* print config */
        fmt::print("\n * targetResolution: ");
        if (targetRes == 0)
        {
            fmt::print("Max Available\n");
        }
        else if (targetRes == -1)
        {
            fmt::print(fmt::fg(fmt::color::cyan), "Lowest Available\n");
        }
        else
        {
            fmt::print(fmt::fg(fmt::color::cyan), fmt::format("{}p\n", targetRes));
        }
        fmt::print(" * audioLanguage: ");
        fmt::print(fmt::fg(fmt::color::cyan), fmt::format("{}\n", audioLang == "jp" ? "Japanese" : audioLang == "zh" ? "Chinese" : "English"));
        fmt::print(" * exportLinks: ");
        exportLinks ? fmt::print(fmt::fg(fmt::color::cyan), "true") : fmt::print("false");
        (exportLinks && export_filename != "links.txt") ? fmt::print(fmt::fg(fmt::color::cyan), fmt::format(" [{}]\n", export_filename)) : fmt::print("\n");
        fmt::print(" * createZip: ", createZip);
        createZip ? fmt::print(fmt::fg(fmt::color::cyan), "true") : fmt::print("false\n");
        if (createZip && removeSource)
        {
            fmt::print(fmt::fg(fmt::color::cyan), " [Remove Source]\n");
        }
        else if (createZip && !removeSource)
        {
            std::cout << std::endl;
        }

        /* Requested Episodes Range */
        if (isSeries)
        {
            fmt::print(" * episodesRange: ");
            isAllEpisodes ? fmt::print("All") : fmt::print(fmt::fg(fmt::color::cyan), vectorToString(episodes));
            fmt::print("\n");
        }
        /* Request Metadata */
        std::string series_name = extract_link_metadata(link, isSeries);

        /* Extract Links */
        const std::vector<std::map<std::string, std::string>> epData = extract_link_content(link, episodes, targetRes, audioLang, isSeries, isAllEpisodes);

        std::vector<std::string> directLinks;
        int logEpNum = isAllEpisodes ? 1 : episodes[0];
        for (int i = 0; i < epData.size(); ++i)
        {
            fmt::print("\n\r * Processing :");
            fmt::print(fmt::fg(fmt::color::cyan), fmt::format(" EP{}", padIntWithZero(logEpNum)));
            std::string link = kwikpahe.extract_kwik_link(epData[i].at("dPaheLink"));
            for (int i = 0; i < 3; ++i)
            {
                fmt::print("{}{}{}", MOVE_UP, CLEAR_LINE, CURSOR_START);
            }
            fmt::print("\r * Processing : EP{}", padIntWithZero(logEpNum));
            if (link.empty())
            {
                fmt::print(fmt::fg(fmt::color::indian_red), " FAIL!");
            }
            else
            {
                directLinks.push_back(link);
                fmt::print(fmt::fg(fmt::color::lime_green), " OK!");
            }
            logEpNum++;
        }

        if (exportLinks)
        {
            std::ofstream exportfile(export_filename);
            if (exportfile.is_open())
            {
                for (auto &link : directLinks)
                {
                    exportfile << link << "\n";
                }
                exportfile.close();
            }
            fmt::print("\n\n * Exported : {}\n\n", export_filename);
        }
        else
        {
            /* sanitize anime name for windows support */
            std::string dirName = sanitizeForWindowsPath(series_name);
            Downloader downloader(directLinks);
            downloader.setDownloadDirectory(dirName);
            downloader.startDownloads();
            fmt::print("\n\x1b[2K\r");

            /* create zip of downloaded items */
            if (createZip)
            {
                /* Create Zip logic */
                auto enhanced_progress = [](size_t current, size_t total, const std::string &file, size_t bytes_done, size_t bytes_total)
                {
                    double file_progress = total > 0 ? (double(current) / total) * 100.0 : 0.0;
                    double byte_progress = bytes_total > 0 ? (double(bytes_done) / bytes_total) * 100.0 : 0.0;

                    /* Create progress bar */
                    const int bar_width = 30;
                    int filled = static_cast<int>(file_progress * bar_width / 100.0);

                    std::ostringstream progress_stream;

                    progress_stream << "\r * [";
                    for (int i = 0; i < bar_width; ++i)
                    {
                        if (i < filled)
                            progress_stream << "=";
                        else if (i == filled)
                            progress_stream << ">";
                        else
                            progress_stream << " ";
                    }
                    progress_stream << "] " << std::fixed << std::setprecision(1) << "(" << current << "/" << total << ") " << file_progress << "% ";
                    std::string new_line = progress_stream.str();

                    std::cout << new_line << std::flush;
                };

                std::cout << "\n * Zipping..\n";

                /* Use the enhanced progress callback */
                std::string zipName = replaceSpacesWithUnderscore(dirName);
                bool success = ZipUtils::zip_directory(
                    fmt::format("./{}", dirName),
                    fmt::format("{}.zip", zipName),
                    removeSource,
                    enhanced_progress
                );

                for (int i = 0; i < 2; ++i)
                {
                    fmt::print("{}{}{}", CLEAR_LINE, MOVE_UP, CURSOR_START);
                }

                fmt::print("\n * Zipping : ");
                (success ? fmt::print(fmt::fg(fmt::color::lime_green), "OK ") : fmt::print(fmt::fg(fmt::color::indian_red), "FAIL!\n"));
                if (success)
                {
                    std::cout << "(";
                    fmt::print(fmt::fg(fmt::color::cyan), fmt::format("{}.zip", zipName));
                    std::cout << ")" << std::endl;
                }
            }
            std::cout << std::endl;
        }
    }
}
