#pragma once
#include <string>

class HttpClient {
public:
    // Simple HTTP/HTTPS GET using WinINet.
    // Returns the response body as a string, or "{}" on error.
    static std::string Get(const std::string& url);

    // HTTP POST with JSON body (WinINet).
    // Sends Content-Type: application/json and returns the response body.
    static std::string PostJson(const std::string& url, const std::string& jsonBody);
};

