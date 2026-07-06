#pragma once

#ifndef COOKIEHELPER_HPP
#define COOKIEHELPER_HPP

#include <string>

namespace AnimepaheCLI
{
    /* Returns a Cookie header value (semicolon-joined name=value pairs) for the
     * URL's host, or "" if nothing matches. Sources, in order:
     *   1. KWIK_COOKIE env var (for kwik.* hosts only — kept for back-compat).
     *   2. cookiesByHost map in ~/Downloads/animepahe-cli/cookie.json, written
     *      by the companion browser extension. Matches by exact host then by
     *      suffix (so vault-07.kwik.cx picks up the "kwik.cx" entry). */
    std::string cookieForUrl(const std::string &url);

    /* User-Agent the extension recorded (so it matches what Cloudflare expects
     * for the cookie), KWIK_UA env override if set, else a Chrome-ish default.
     * Sticky once loaded. */
    std::string preferredUserAgent();
}

#endif
