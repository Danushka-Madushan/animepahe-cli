#include <cxxopts.hpp>
#include <fmt/core.h>
#include <fmt/color.h>
#include <string>
#include <utils.hpp>
#include <animepahe.hpp>
#ifdef _WIN32
#include <githubupdater.hpp>
#else
#include "include/githubupdater_stub.h"
#endif

using namespace AnimepaheCLI;

int main(int argc, char *argv[])
{
    /**
     * -l, --link
     * input anime series link or a single episode link
     * -e, --episodes
     * all,1-15 all means full series, or episode range
     * -x, --export
     * saves all download links to a text file
     * -f, --filename
     * set custom export filename
     * -q, --quality
     * set target quality, if available it will select otherwise fall back to maximum
     * -z, --zip
     * creates a zip from downloaded items
     * --rm-source
     * remove source files after zipping
     * --update
     * self update to the latest version */

    cxxopts::Options options("animepahe-cli", "AnimePahe CLI Downloader");
    options.add_options()
    ("l,link", "Input anime series link or a single episode link", cxxopts::value<std::string>())
    ("e,episodes", "Specify episodes to download (all, 1-15)", cxxopts::value<std::string>()->default_value("all"))
    ("q,quality", "Set target quality", cxxopts::value<int>()->default_value("0"))
    ("x,export", "Export download links to a text file", cxxopts::value<bool>()->default_value("false"))
    ("f,filename", "Custom filname for exported file", cxxopts::value<std::string>()->default_value("links.txt"))
    ("z,zip", "Create a zip from downloaded items", cxxopts::value<bool>()->default_value("false"))
    ("rm-source", "Delete source files after zipping", cxxopts::value<bool>()->default_value("false"))
    ("upgrade", "Update to the latest version")
    ("h,help", "Print usage") ("s,stream","Stream video via FFmpeg","url")
    ("audio-track","Audio track (eng or jpn)",cxxopts::value<std::string>()->default_value("jpn"));


    /* version tag */
    const std::string VERSION = "v0.2.2-beta";

    try
    {
        auto result = options.parse(argc, argv);
        GitHubUpdater updater("Danushka-Madushan", "animepahe-cli", VERSION);

        if (result.count("help"))
        {
            fmt::print("{}\n", options.help());
            return 0;
        }

        if (result.count("upgrade"))
        {
            updater.checkAndUpdate();
            return 0;
        }

        std::string link = result["link"].as<std::string>();
        std::string episodes = result["episodes"].as<std::string>();
        int targetRes = result["quality"].as<int>();
        bool exportLinks = result["export"].as<bool>();
        bool createZip = result["zip"].as<bool>();
        bool removeSource = result["rm-source"].as<bool>();
        std::string export_filename = result["filename"].as<std::string>();

        if (!isFullSeriesURL(link) && !isEpisodeURL(link))
        {
            throw std::runtime_error("Invalid link format. Please provide a valid AnimePahe series or episode link.");
        }
        if (!isValidEpisodeRangeFormat(episodes))
        {
            throw std::runtime_error("Invalid episode range format. Use 'all' or '1-15'.");
        }
        if (!isValidTxtFilename(export_filename))
        {
            throw std::runtime_error(fmt::format("{} is not valid for -f,--filename [filename]", export_filename));
        }
        if (targetRes < -1)
        {
            throw std::runtime_error(fmt::format("{} is not valid for -q,--quality [0-max,-1-min,720|360]", targetRes));
        }
        if (exportLinks && createZip)
        {
            /* exporting method takes prority */
            createZip = false;
            if (removeSource)
            {
                removeSource = false;
            }
        }

        fmt::print("\n * Animepahe-CLI ({}) https://github.com/Danushka-Madushan/animepahe-cli \n", VERSION);

        /* check for updates */
        auto release = updater.checkForUpdate();
        if (release)
        {
            fmt::print("\n * Update available : ");
            fmt::print(fmt::fg(fmt::color::lime_green), release->tag_name);
            fmt::print(" (use --upgrade to self update)");
            fmt::print("\n");
        }

        // Create an instance of Animepahe and call the extractor method
        Animepahe animepahe;
        animepahe.extractor(
            isFullSeriesURL(link),
            link,
            targetRes,
            episodes == "all",
            episodes == "all" ? std::vector<int>() : parseEpisodeRange(episodes),
            export_filename,
            exportLinks,
            createZip,
            removeSource
        );
    }
    catch (const cxxopts::exceptions::option_has_no_value)
    {
        fmt::print("\n --link,-l and --episodes,-e must be provided!\n\n");
        return 1;
    }
    catch (const cxxopts::exceptions::missing_argument)
    {
        fmt::print("\n Usage: -l,--link \"https://animepahe.si/anime/....\" -e,--episodes [all,1-12] -q,--quality [0-max,-1-min,720|360] -x,--export, -f,--filename [filename] -z,--zip, --rm-source, --upgrade\n\n");
        return 1;
    }
    catch (const std::runtime_error &e)
    {
        fmt::print("\n\n * ");
        fmt::print(fmt::fg(fmt::color::indian_red), "ERROR :");
        fmt::print(" {} \n\n", e.what());
        return 1;
    }
    catch (const std::exception &e)
    {
        fmt::print("\n\n * ");
        fmt::print(fmt::fg(fmt::color::indian_red), "ERROR :");
        fmt::print(" {} \n\n", e.what());
        return 1;
    }

    return 0;
}
