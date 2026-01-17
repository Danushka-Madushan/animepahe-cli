#include <cpr/cpr.h>
#include <filesystem>
#include <vector>
#include <string>

class Downloader {
public:
    Downloader(const std::vector<std::map<std::string, std::string>>& urls);
    void setDownloadDirectory(const std::string& dir);
    void startDownloads();

private:
    std::vector<std::map<std::string, std::string>> urls_;
    std::string download_dir_;
    static const int MAX_RETRIES = 3;

    std::string extractFilename(const std::string& url) const;
    bool downloadFile(const std::string &url, const std::string &filepath, const std::string &referer);
    bool downloadFileWithRetry(const std::string& url, const std::string& filepath, int retries, const std::string &referer);
};
