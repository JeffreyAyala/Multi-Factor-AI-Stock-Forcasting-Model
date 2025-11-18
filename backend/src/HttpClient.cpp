#include "HttpClient.hpp"
#include <windows.h>
#include <wininet.h>
#include <string>

#pragma comment(lib, "wininet.lib")

std::string HttpClient::Get(const std::string& url)
{
    // Open WinINet
    HINTERNET hInternet = InternetOpenA(
        "StockPredictor/1.0",
        INTERNET_OPEN_TYPE_PRECONFIG,
        NULL,
        NULL,
        0
    );

    if (!hInternet) {
        return "{}";
    }

    // Flags: reload, no cache, allow HTTPS
    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (url.rfind("https://", 0) == 0) {
        flags |= INTERNET_FLAG_SECURE;
    }

    HINTERNET hUrl = InternetOpenUrlA(
        hInternet,
        url.c_str(),
        NULL,
        0,
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

    do {
        if (!InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) || bytesRead == 0) {
            break;
        }
        result.append(buffer, bytesRead);
    } while (bytesRead > 0);

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    if (result.empty()) {
        return "{}";
    }

    return result;
}

// ---------------------------------------------------------
// POST JSON using WinINet
// ---------------------------------------------------------
std::string HttpClient::PostJson(const std::string& url, const std::string& jsonBody)
{
    // 1. Open WinINet session
    HINTERNET hInternet = InternetOpenA(
        "StockPredictor/1.0",
        INTERNET_OPEN_TYPE_PRECONFIG,
        NULL,
        NULL,
        0
    );
    if (!hInternet) {
        return "{}";
    }

    // 2. Crack URL into components
    URL_COMPONENTSA urlComp;
    memset(&urlComp, 0, sizeof(urlComp));
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

    // 3. Open connection
    HINTERNET hConnect = InternetConnectA(
        hInternet,
        urlComp.lpszHostName,
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

    // 4. Open HTTP POST request
    HINTERNET hRequest = HttpOpenRequestA(
        hConnect,
        "POST",
        urlComp.lpszUrlPath,
        NULL,
        NULL,
        NULL,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE,
        0
    );
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "{}";
    }

    // 5. JSON header
    const char* headers = "Content-Type: application/json\r\n";

    // 6. Send request with JSON body
    BOOL bSend = HttpSendRequestA(
        hRequest,
        headers,
        (DWORD)std::strlen(headers),
        (LPVOID)jsonBody.c_str(),
        (DWORD)jsonBody.size()
    );

    if (!bSend) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "{}";
    }

    // 7. Read response
    std::string response;
    char buffer[4096];
    DWORD bytesRead = 0;

    do {
        if (!InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) || bytesRead == 0)
            break;

        response.append(buffer, bytesRead);
    } while (bytesRead > 0);

    // Cleanup
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    if (response.empty()) {
        return "{}";
    }

    return response;
}



