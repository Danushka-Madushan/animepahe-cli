#pragma once

#ifndef KWIKPAHE_HPP
#define KWIKPAHE_HPP

#include <string>
#include <map>

namespace AnimepaheCLI
{
    class KwikPahe
    {
    private:
        int _0xe16c(const std::string &IS, int Iy, int ms);
        std::string decodeJSStyle(const std::string &Hb, int zp, const std::string &Wg, int Of, int Jg, int gj_placeholder);
        std::string fetch_kwik_dlink(const std::string& kwikLink, int retries = 5); 
        std::string fetch_kwik_direct(const std::string &kwikLink, const std::string &token, const std::string &kwik_session);
    public:
        std::map<std::string, std::string> extract_kwik_link(const std::string& link);
    };
}

#endif
