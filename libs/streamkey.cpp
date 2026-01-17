#include <streamkey.hpp>
#include <utils.hpp>
#include <iostream>
#include <string>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace AnimepaheCLI
{
    std::string getStreamKey(const std::string &directUrl, const std::string &referer)
    {
        /* Create JSON Object */
        json streamKeyObj = {
            {"directUrl", directUrl},
            {"referer", referer}};

        /* Serialize to string */
        std::string jsonStr = streamKeyObj.dump();

        /* Encode to standard Base64 */
        std::string b64 = base64_encode(jsonStr);

        /* Make it URL-Safe */
        std::replace(b64.begin(), b64.end(), '+', '-');
        std::replace(b64.begin(), b64.end(), '/', '_');
        b64.erase(std::remove(b64.begin(), b64.end(), '='), b64.end());

        return b64;
    }
}
