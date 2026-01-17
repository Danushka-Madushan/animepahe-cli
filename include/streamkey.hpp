#pragma once

#ifndef STREAMKEY_HPP
#define STREAMKEY_HPP

#include <string>

namespace AnimepaheCLI
{
    std::string getStreamKey(const std::string &directUrl, const std::string &referer);
}

#endif
