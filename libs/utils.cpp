#include <re2/re2.h>
#include <pugixml.hpp>
#include <set>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <string_view>
#include <string>
#include <regex>
#include <unordered_set>

namespace AnimepaheCLI
{

    std::string replaceSpacesWithUnderscore(std::string text) {
        std::replace(text.begin(), text.end(), ' ', '_');
        return text;
    }

    std::string sanitizeForWindowsPath(std::string name)
    {
        // Replace invalid characters with '_'
        static const std::regex invalid(R"([<>:"/\\|?*\x00-\x1F])");
        name = std::regex_replace(name, invalid, "_");

        // Remove trailing spaces or dots
        while (!name.empty() && (name.back() == ' ' || name.back() == '.'))
            name.pop_back();

        // Reserved Windows names (case-insensitive)
        static const std::unordered_set<std::string> reserved = {
            "CON", "PRN", "AUX", "NUL",
            "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
            "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};

        std::string upperName = name;
        std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);
        if (reserved.count(upperName))
        {
            name = "_" + name; // Prefix to avoid conflict
        }

        // Edge case: empty name after cleaning
        if (name.empty())
        {
            name = "_unnamed";
        }

        return name;
    }

    int getPage(int number)
    {
        return std::max(1, (number + 29) / 30);
    }

    bool isValidTxtFilename(const std::string& filename) {
        // Disallow any path separator
        if (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos) {
            return false;
        }

        // Check with RE2 regex
        // Pattern: Only letters, digits, underscores, hyphens, dots, ends with ".txt"
        static const RE2 pattern(R"(^[a-zA-Z0-9_\-\.]+\.\S*$)");

        return RE2::FullMatch(filename, pattern);
    }

    std::vector<int> getPaginationRange(int start, int end)
    {
        int start_page = getPage(start);
        int end_page = getPage(end);
        std::vector<int> pages;

        for (int i = start_page; i <= end_page; ++i)
        {
            pages.push_back(i);
        }

        return pages;
    }

    std::string sanitize_utf8(const std::string &input)
    {
        std::string output;
        output.reserve(input.size());

        const unsigned char *data = reinterpret_cast<const unsigned char *>(input.data());
        size_t len = input.size();

        for (size_t i = 0; i < len;)
        {
            unsigned char byte = data[i];

            if (byte <= 0x7F)
            {
                output += byte;
                i++;
            }
            else if ((byte >> 5) == 0x6 && i + 1 < len &&
                     (data[i + 1] & 0xC0) == 0x80)
            {
                output.append(reinterpret_cast<const char *>(&data[i]), 2);
                i += 2;
            }
            else if ((byte >> 4) == 0xE && i + 2 < len &&
                     (data[i + 1] & 0xC0) == 0x80 &&
                     (data[i + 2] & 0xC0) == 0x80)
            {
                output.append(reinterpret_cast<const char *>(&data[i]), 3);
                i += 3;
            }
            else if ((byte >> 3) == 0x1E && i + 3 < len &&
                     (data[i + 1] & 0xC0) == 0x80 &&
                     (data[i + 2] & 0xC0) == 0x80 &&
                     (data[i + 3] & 0xC0) == 0x80)
            {
                output.append(reinterpret_cast<const char *>(&data[i]), 4);
                i += 4;
            }
            else
            {
                // invalid byte, skip
                i++;
            }
        }

        return output;
    }

    bool isFullSeriesURL(const std::string &url)
    {
        // Accept both legacy .ru and new primary .si domains
        return RE2::FullMatch(url, R"(^https:\/\/animepahe\.(ru|si)\/anime\/[a-f0-9\-]{36}$)");
    }

    bool isEpisodeURL(const std::string &url)
    {
        // Accept both legacy .ru and new primary .si domains
        return RE2::FullMatch(url, R"(^https:\/\/animepahe\.(ru|si)\/play\/[a-f0-9\-]{36}\/[a-f0-9]{64}$)");
    }

    bool isValidEpisodeRangeFormat(const std::string &input)
    {
        if (input == "all")
            return true;

        int start, end;
        // Support single episode numbers like "3" or ranges like "1-12" or "3-3"
        if (RE2::FullMatch(input, R"((\d+))", &start))
            return start > 0;

        return RE2::FullMatch(input, R"((\d+)-(\d+))", &start, &end) && start > 0 && end >= start;
    }

    /* parse episodes 1-15 or single episode like 3 */
    std::vector<int> parseEpisodeRange(const std::string &input)
    {
        int start, end;

        // Check for single episode number like "3"
        if (RE2::FullMatch(input, R"((\d+))", &start))
        {
            if (start > 0)
                return {start, start}; // Treat as range 3-3
        }

        // Check for range like "1-12" or "3-3"
        if (RE2::FullMatch(input, R"((\d+)-(\d+))", &start, &end))
        {
            if (start > 0 && end >= start)
            {
                return {start, end};
            }
        }
        throw std::invalid_argument("Invalid episode range format");
    }

    std::string unescape_html_entities(const std::string &input)
    {
        pugi::xml_document doc;

        // Wrap string in dummy XML structure
        std::string xml = "<root>" + input + "</root>";
        pugi::xml_parse_result result = doc.load_string(xml.c_str());

        if (!result)
        {
            std::cerr << "Failed to parse XML: " << result.description() << "\n";
            return input; // fallback to original
        }

        return doc.child("root").text().get(); // decoded text
    }

    std::string padIntWithZero(int num)
    {
        std::ostringstream oss;
        oss << std::setw(2) << std::setfill('0') << num;
        return oss.str();
    }

    std::string base64_encode(const std::string &in)
    {
        static const char lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        int val = 0, valb = -6;
        for (unsigned char c : in) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                out.push_back(lookup[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6) out.push_back(lookup[((val << 8) >> (valb + 8)) & 0x3F]);
        while (out.size() % 4) out.push_back('=');
        return out;
    }
}
