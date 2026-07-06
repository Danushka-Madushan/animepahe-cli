#include <cookiehelper.hpp>

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string>

namespace AnimepaheCLI
{
    namespace
    {
        constexpr const char *DEFAULT_USER_AGENT =
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/138.0.0.0 Safari/537.36 Edg/138.0.0.0";

        std::string envOrEmpty(const char *name)
        {
            const char *v = std::getenv(name);
            return (v && *v) ? std::string(v) : std::string();
        }

        std::filesystem::path helperFilePath()
        {
            std::string home;
#ifdef _WIN32
            home = envOrEmpty("USERPROFILE");
#else
            home = envOrEmpty("HOME");
#endif
            if (home.empty())
                return {};
            return std::filesystem::path(home) / "Downloads" / "animepahe-cli" / "cookie.json";
        }

        struct State
        {
            std::map<std::string, std::string> cookiesByHost;
            std::string userAgent;
        };

        void loadInto(State &s)
        {
            auto path = helperFilePath();
            std::error_code ec;
            if (path.empty() || !std::filesystem::exists(path, ec))
                return;

            std::ifstream f(path);
            if (!f)
                return;

            try
            {
                nlohmann::json j;
                f >> j;

                if (j.contains("cookiesByHost") && j["cookiesByHost"].is_object())
                {
                    for (auto &kv : j["cookiesByHost"].items())
                    {
                        if (kv.value().is_string() && !kv.key().empty())
                            s.cookiesByHost[kv.key()] = kv.value().get<std::string>();
                    }
                }
                /* Legacy v1.0 format wrote a single "cookie" field for kwik.cx. */
                else if (j.contains("cookie") && j["cookie"].is_string())
                {
                    s.cookiesByHost["kwik.cx"] = j["cookie"].get<std::string>();
                }

                if (j.contains("userAgent") && j["userAgent"].is_string())
                    s.userAgent = j["userAgent"].get<std::string>();
            }
            catch (...)
            {
                /* Malformed file — fall through with empty state. */
            }
        }

        const State &state()
        {
            static State s;
            static std::once_flag flag;
            std::call_once(flag, [&] { loadInto(s); });
            return s;
        }

        std::string hostOfUrl(const std::string &url)
        {
            auto schemeEnd = url.find("://");
            if (schemeEnd == std::string::npos)
                return {};
            auto hostStart = schemeEnd + 3;
            auto hostEnd = url.find('/', hostStart);
            if (hostEnd == std::string::npos)
                hostEnd = url.size();
            return url.substr(hostStart, hostEnd - hostStart);
        }

        bool isKwikHost(const std::string &host)
        {
            const std::string suffix = ".kwik.cx";
            return host == "kwik.cx" ||
                   (host.size() > suffix.size() &&
                    host.compare(host.size() - suffix.size(), suffix.size(), suffix) == 0);
        }

        std::string findHostCookies(const std::string &host)
        {
            const auto &map = state().cookiesByHost;

            auto it = map.find(host);
            if (it != map.end())
                return it->second;

            /* Suffix match: "vault.kwik.cx" picks up the "kwik.cx" entry. */
            for (const auto &kv : map)
            {
                const std::string &k = kv.first;
                if (host.size() > k.size() + 1 &&
                    host[host.size() - k.size() - 1] == '.' &&
                    host.compare(host.size() - k.size(), k.size(), k) == 0)
                {
                    return kv.second;
                }
            }
            return {};
        }
    }

    std::string cookieForUrl(const std::string &url)
    {
        std::string host = hostOfUrl(url);
        if (host.empty())
            return {};

        if (isKwikHost(host))
        {
            std::string env = envOrEmpty("KWIK_COOKIE");
            if (!env.empty())
                return env;
        }

        return findHostCookies(host);
    }

    std::string preferredUserAgent()
    {
        std::string env = envOrEmpty("KWIK_UA");
        if (!env.empty())
            return env;
        const std::string &ua = state().userAgent;
        return ua.empty() ? std::string(DEFAULT_USER_AGENT) : ua;
    }
}
