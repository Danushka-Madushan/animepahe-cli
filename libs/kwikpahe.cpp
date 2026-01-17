#include <kwikpahe.hpp>
#include <utils.hpp>
#include <cpr/cpr.h>
#include <fmt/core.h>
#include <fmt/color.h>
#include <re2/re2.h>
#include <string>
/* DECODER LIBS */
#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <cctype>

namespace AnimepaheCLI
{
    std::string encodedString = "XXX"; // Encoded string
    int zp = 17;                       // Not used in decoding
    std::string alphabetKey = "XXX";   // Alphabet key
    int offset = 1;                    // Offset
    int base = 5;                      // Base
    int placeholder = 24;              // Placeholder (unused)

    std::string baseAlphabet = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/";

    int KwikPahe::_0xe16c(const std::string &IS, int Iy, int ms)
    {
        std::string h = baseAlphabet.substr(0, Iy);
        std::string i = baseAlphabet.substr(0, ms);

        // Decode string IS from base Iy to int j
        int j = 0;
        for (int idx = 0; idx < IS.size(); ++idx)
        {
            char ch = IS[IS.size() - 1 - idx]; // reverse order
            size_t pos = h.find(ch);
            if (pos != std::string::npos)
            {
                j += static_cast<int>(pos) * static_cast<int>(std::pow(Iy, idx));
            }
        }

        // Convert int j to base ms string
        if (j == 0)
            return i[0];

        std::string k;
        while (j > 0)
        {
            k = i[j % ms] + k;
            j /= ms;
        }

        return std::stoi(k);
    }

    std::string KwikPahe::decodeJSStyle(const std::string &Hb, int zp, const std::string &Wg, int Of, int Jg, int gj_placeholder)
    {
        std::string gj;

        for (size_t i = 0; i < Hb.size(); ++i)
        {
            std::string s;
            while (Hb[i] != Wg[Jg])
            {
                s += Hb[i];
                i++;
                if (i >= Hb.size())
                    break;
            }

            for (size_t j = 0; j < Wg.size(); ++j)
            {
                std::string from(1, Wg[j]);
                std::string to = std::to_string(j);
                size_t pos;
                while ((pos = s.find(from)) != std::string::npos)
                {
                    s.replace(pos, 1, to);
                }
            }

            int code = _0xe16c(s, Jg, 10) - Of;
            gj += static_cast<char>(code);
        }

        return gj;
    }

    std::string KwikPahe::fetch_kwik_direct(const std::string &kwikLink, const std::string &token, const std::string &kwik_session)
    {
        // Set up cookies
        cpr::Header headers = cpr::Header{
            {"referer", kwikLink},
            {"cookie", "kwik_session=" + kwik_session},
        };
        // Set up form data
        cpr::Payload data = cpr::Payload{{"_token", token}};

        // Make POST request with redirects disabled
        cpr::Response response = cpr::Post(
            cpr::Url{kwikLink},
            headers,
            data,
            cpr::Redirect(false),
            cpr::HttpVersion{cpr::HttpVersionCode::VERSION_1_1}
        );

        // Check if status code is 302 (redirect)
        if (response.status_code == 302)
        {
            // Extract the redirect location from the response header
            std::string redirectLocation;
            re2::StringPiece rawHeader(response.raw_header);
            if (RE2::FindAndConsume(&rawHeader, R"re(ocation:\s*(https?://\S+))re", &redirectLocation))
            {
                return redirectLocation;
            }
            throw std::runtime_error(fmt::format("Redirect Location not found in response from {}", kwikLink));
        }
        else
        {
            throw std::runtime_error(fmt::format("Redirect Location not found in response from {}", kwikLink));
        }
    }

    std::string KwikPahe::fetch_kwik_dlink(const std::string &kwikLink, int retries)
    {
        if (retries <= 0)
        {
            throw std::runtime_error(fmt::format("Kwik fetch failed: exceeded retry limit : {}", kwikLink));
        }

        cpr::Response response = cpr::Get(cpr::Url{kwikLink});
        if (response.status_code != 200)
        {
            throw std::runtime_error(fmt::format("Failed to Get Kwik from {}, StatusCode: {}", kwikLink, response.status_code));
        }

        // Clean the response text
        std::string cleanText = response.text;
        RE2::GlobalReplace(&cleanText, R"((\r\n|\r|\n))", "");
        
        // Extract session from headers
        std::string kwik_session;
        re2::StringPiece input(response.raw_header);
        RE2::FindAndConsume(&input, R"re(kwik_session=([^;]*);)re", &kwik_session);

        std::string link, token;
        std::string directLink;

        // Try to extract encoded parameters - use separate StringPiece for each attempt
        re2::StringPiece encode_text(cleanText);
        std::string temp_encoded, temp_alphabet, temp_offset_str, temp_base_str;
        
        bool found_encoded = RE2::FindAndConsume(
            &encode_text,
            R"re(\(\s*"([^",]*)"\s*,\s*\d+\s*,\s*"([^",]*)"\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*\d+[a-zA-Z]?\s*\))re",
            &temp_encoded, &temp_alphabet, &temp_offset_str, &temp_base_str
        );

        if (!found_encoded || temp_encoded.empty() || temp_alphabet.empty())
        {
            return fetch_kwik_dlink(kwikLink, retries - 1);
        }

        // Update global variables only if extraction succeeded
        encodedString = temp_encoded;
        alphabetKey = temp_alphabet;  
        offset = std::stoi(temp_offset_str);
        base = std::stoi(temp_base_str);

        try 
        {
            std::string decodedString = decodeJSStyle(encodedString, zp, alphabetKey, offset, base, placeholder);
            
            // Use fresh StringPiece objects for each search
            re2::StringPiece link_search(decodedString);
            re2::StringPiece token_search(decodedString);
            
            bool found_link = RE2::FindAndConsume(&link_search, R"re("(https?://kwik\.[^/\s"]+/[^/\s"]+/[^"\s]*)")re", &link);
            bool found_token = RE2::FindAndConsume(&token_search, R"re(name="_token"[^"]*"(\S*)">)re", &token);

            if (!found_link || !found_token || link.empty() || token.empty())
            {
                return fetch_kwik_dlink(kwikLink, retries - 1);
            }

            directLink = fetch_kwik_direct(link, token, kwik_session);
        }
        catch (const std::exception& e)
        {
            return fetch_kwik_dlink(kwikLink, retries - 1);
        }

        return directLink;
    }

    std::map<std::string, std::string> KwikPahe::extract_kwik_link(const std::string &link)
    {
        fmt::print("\n\r * Extracting Kwik Link...");
        cpr::Response response = cpr::Get(cpr::Url{link});
        if (response.status_code != 200)
        {
            throw std::runtime_error(fmt::format("Failed to Get Kwik from {}, StatusCode: {}", link, response.status_code));
        }
        
        std::string cleanText = response.text;
        RE2::GlobalReplace(&cleanText, R"((\r\n|\r|\n))", "");
        cleanText = sanitize_utf8(cleanText);
        
        std::string kwikLink;
        
        // First attempt: direct link extraction
        re2::StringPiece normal_search(cleanText);
        bool found_direct = RE2::FindAndConsume(&normal_search, R"re("(https?://kwik\.[^/\s"]+/[^/\s"]+/[^"\s]*)")re", &kwikLink);
        
        if (!found_direct || kwikLink.empty())
        {
            // Second attempt: decode and extract
            re2::StringPiece encode_search(cleanText);
            std::string temp_encoded, temp_alphabet, temp_offset_str, temp_base_str;
            
            bool found_params = RE2::FindAndConsume(
                &encode_search,
                R"re(\(\s*"([^",]*)"\s*,\s*\d+\s*,\s*"([^",]*)"\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*\d+[a-zA-Z]?\s*\))re",
                &temp_encoded, &temp_alphabet, &temp_offset_str, &temp_base_str
            );

            if (!found_params)
            {
                throw std::runtime_error(fmt::format("Failed to extract encoding parameters from {}", link));
            }

            try 
            {
                int temp_offset = std::stoi(temp_offset_str);
                int temp_base = std::stoi(temp_base_str);
                
                std::string decodedString = decodeJSStyle(temp_encoded, zp, temp_alphabet, temp_offset, temp_base, placeholder);
                re2::StringPiece decoded_search(decodedString);
                
                bool found_decoded = RE2::FindAndConsume(&decoded_search, R"re("(https?://kwik\.[^/\s"]+/[^/\s"]+/[^"\s]*)")re", &kwikLink);
                
                if (!found_decoded || kwikLink.empty())
                {
                    throw std::runtime_error(fmt::format("Failed to extract Kwik link from decoded content"));
                }
                
                RE2::Replace(&kwikLink, R"re((https:\/\/kwik\.[^\/]+\/)d\/)re", "\\1f/");
            }
            catch (const std::exception& e)
            {
                throw std::runtime_error(fmt::format("Failed to decode and extract Kwik link: {}", e.what()));
            }
        }

        fmt::print("\r * Extracting Kwik Link :");
        fmt::print(fmt::fg(fmt::color::lime_green), " OK!\n");
        fmt::print(" * Fetching Kwik Direct Link...");
        
        const std::string directLink = fetch_kwik_dlink(kwikLink);
        std::map<std::string, std::string> dlMap = {
            { "referer", kwikLink },
            { "directLink", directLink }
        };

        fmt::print("\r * Fetching Kwik Direct Link :");
        fmt::print(fmt::fg(fmt::color::lime_green), " OK!\n");

        return dlMap;
    }
}
