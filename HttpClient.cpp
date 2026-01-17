#include "HttpClient.hpp"
#include <windows.h>
#include <wininet.h>
#include <string>
#include <cstring>

#pragma comment(lib, "wininet.lib")

// ============================================================================
// HTTP GET
// ============================================================================
std::string HttpClient::Get(const std::string& url)
{
    HINTERNET hInternet = InternetOpenA(
        "StockPredictor/1.0",
        INTERNET_OPEN_TYPE_PRECONFIG,
        NULL,
        NULL,
        0
    );
    if (!hInternet) return "{}";

    DWORD flags = INTERNET_FLAG_RELOAD |
        INTERNET_FLAG_NO_CACHE_WRITE |
        INTERNET_FLAG_PRAGMA_NOCACHE |
        INTERNET_FLAG_KEEP_CONNECTION |
        INTERNET_FLAG_NO_COOKIES;

    if (url.rfind("https://", 0) == 0)
        flags |= INTERNET_FLAG_SECURE;

    HINTERNET hUrl = InternetOpenUrlA(
        hInternet,
        url.c_str(),
        NULL, 0,
        flags,
        0
    );

    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return "{}";
    }

    std::string result;
    char buffer[4096];
    DWORD bytesRead = 0;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        result.append(buffer, bytesRead);
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    return result.empty() ? "{}" : result;
}



// ============================================================================
// HTTP POST (JSON) — FINAL STABLE VERSION
// ============================================================================
std::string HttpClient::PostJson(const std::string& url, const std::string& jsonBody)
{
    HINTERNET hInternet = InternetOpenA(
        "StockPredictor/1.0",
        INTERNET_OPEN_TYPE_PRECONFIG,
        NULL,
        NULL,
        0
    );
    if (!hInternet) return "{}";

    URL_COMPONENTSA urlComp{};
    urlComp.dwStructSize = sizeof(URL_COMPONENTSA);

    char host[256];
    char path[1024];

    urlComp.lpszHostName = host;
    urlComp.dwHostNameLength = sizeof(host);

    urlComp.lpszUrlPath = path;
    urlComp.dwUrlPathLength = sizeof(path);

    if (!InternetCrackUrlA(url.c_str(), 0, 0, &urlComp)) {
        InternetCloseHandle(hInternet);
        return "{}";
    }

    // ---------------------------------------------------------
    // Force null-termination (IMPORTANT)
    // ---------------------------------------------------------
    if (urlComp.dwHostNameLength < sizeof(host))
        host[urlComp.dwHostNameLength] = '\0';
    else
        host[sizeof(host) - 1] = '\0';

    if (urlComp.dwUrlPathLength < sizeof(path))
        path[urlComp.dwUrlPathLength] = '\0';
    else
        path[sizeof(path) - 1] = '\0';


    // HTTPS
    DWORD flags =
        INTERNET_FLAG_RELOAD |
        INTERNET_FLAG_NO_CACHE_WRITE |
        INTERNET_FLAG_KEEP_CONNECTION |
        INTERNET_FLAG_NO_COOKIES |
        INTERNET_FLAG_PRAGMA_NOCACHE;

    if (url.rfind("https://", 0) == 0)
        flags |= INTERNET_FLAG_SECURE;


    HINTERNET hConnect = InternetConnectA(
        hInternet,
        host,
        urlComp.nPort,
        NULL,
        NULL,
        INTERNET_SERVICE_HTTP,
        0,
        0
    );

    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return "{}";
    }

    HINTERNET hRequest = HttpOpenRequestA(
        hConnect,
        "POST",
        path,
        "HTTP/1.1",
        NULL,
        NULL,
        flags,
        0
    );

    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "{}";
    }

    const char* headers =
        "Content-Type: application/json\r\n"
        "Accept: application/json\r\n";

    BOOL ok = HttpSendRequestA(
        hRequest,
        headers,
        (DWORD)strlen(headers),
        (LPVOID)jsonBody.c_str(),
        (DWORD)jsonBody.size()
    );

    if (!ok) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "{}";
    }

    // ---------------------------------------------------------
    // READ RESPONSE (supports chunked response bodies)
    // ---------------------------------------------------------
    std::string response;
    char buffer[4096];
    DWORD bytesRead = 0;

    while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        response.append(buffer, bytesRead);
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    return response.empty() ? "{}" : response;
}




