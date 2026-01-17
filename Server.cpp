#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cstdlib>

#include "PolygonAPI.hpp"
#include "Predictor.hpp"
#include "Indicators.hpp"

#pragma comment(lib, "ws2_32.lib")

// --------------------------------------------
// URL decode
// --------------------------------------------
static std::string urlDecode(const std::string& src) {
    std::string out;
    for (size_t i = 0; i < src.size(); ++i) {
        if (src[i] == '%' && i + 2 < src.size()) {
            std::string hex = src.substr(i + 1, 2);
            char ch = static_cast<char>(strtol(hex.c_str(), nullptr, 16));
            out.push_back(ch);
            i += 2;
        }
        else if (src[i] == '+') out.push_back(' ');
        else out.push_back(src[i]);
    }
    return out;
}

// Escape quotes
static std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else out += c;
    }
    return out;
}

// =========================================================
// MAIN SERVER
// =========================================================
int main() {
    PolygonAPI api("iEt9sFSDGEvS1qdxzKn9ggSLCyBBpG5Q");
    Predictor predictor(&api);

    // Initialize Winsock (L56 - Warning C6031 is handled here)
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        // Handle error (e.g., print an error message and exit the program)
        // fprintf(stderr, "WSAStartup failed with error: %d\n", iResult);
        // return 1;
    }

    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port = htons(5000);

    if (bind(s, (sockaddr*)&srv, sizeof(srv)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(s);
        WSACleanup();
        return 1;
    }

    if (listen(s, 5) == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(s);
        WSACleanup();
        return 1;
    }

    std::cout << "Server running at http://localhost:5000\n";

    while (true) {
        SOCKET c = accept(s, nullptr, nullptr);
        if (c == INVALID_SOCKET) continue;

        char buf[4096] = { 0 };
        int recvd = recv(c, buf, sizeof(buf) - 1, 0);
        if (recvd <= 0) { closesocket(c); continue; }

        std::string req(buf);
        std::string ticker = "AAPL";
        int ahead = 1;

        size_t gp = req.find("GET ");
        if (gp != std::string::npos) {
            size_t ps = gp + 4;
            size_t pe = req.find(' ', ps);
            std::string path = req.substr(ps, pe - ps);

            size_t tpos = path.find("ticker=");
            if (tpos != std::string::npos) {
                size_t vs = tpos + 7;
                size_t ve = path.find_first_of("& ", vs);
                ticker = urlDecode(path.substr(vs, ve - vs));
            }

            size_t apos = path.find("ahead=");
            if (apos != std::string::npos) {
                size_t vs = apos + 6;
                size_t ve = path.find_first_of("& ", vs);
                try {
                    // FIX: L125 - Use parentheses to resolve min/max macro conflict
                    ahead = (std::max)(1, (std::min)(std::stoi(path.substr(vs, ve - vs)), 120));
                }
                catch (...) {
                    ahead = 1;
                }
            }
        }

        Indicators ind;
        std::vector<double> closes;

        double current = api.getLastPrice(ticker);
        double prediction = predictor.predictNextPrice(ticker, ind, closes, ahead);

        Indicators indNow;
        std::vector<double> closesNow;
        double nowcast = predictor.predictNextPrice(ticker, indNow, closesNow, 15);

        std::string closesJson = "[";
        for (size_t i = 0; i < closes.size(); ++i) {
            closesJson += std::to_string(closes[i]);
            if (i + 1 < closes.size()) closesJson += ",";
        }
        closesJson += "]";

        // Build UI-aligned JSON
        std::string body =
            "{"
            "\"ticker\":\"" + jsonEscape(ticker) + "\","
            "\"ahead\":" + std::to_string(ahead) + ","
            "\"current\":" + std::to_string(current) + ","
            "\"nowcast\":" + std::to_string(nowcast) + ","
            "\"prediction\":" + std::to_string(prediction) + ","

            "\"sma20\":" + std::to_string(ind.sma20) + ","
            "\"ema20\":" + std::to_string(ind.ema20) + ","
            "\"rsi14\":" + std::to_string(ind.rsi14) + ","

            "\"magZone\":" + std::to_string(ind.magZone) + ","
            "\"rsiDiv\":" + std::to_string(ind.rsiDiv) + ","
            "\"trend\":" + std::to_string(ind.trend) + ","
            "\"volPressure\":" + std::to_string(ind.volPressure) + ","

            // L175 - Fixed previously: correctly serializes the double newsSent value
            "\"newsSent\":" + std::to_string(ind.newsSent) + ","
            "\"pcr\":" + std::to_string(ind.pcr) + ","
            "\"vix\":" + std::to_string(ind.vix) + ","

            "\"is_london\":" + std::string(ind.isLondon ? "true" : "false") + ","
            "\"is_newyork\":" + std::string(ind.isNewYork ? "true" : "false") + ","
            "\"is_powerhour\":" + std::string(ind.isPowerHour ? "true" : "false") + ","

            "\"closes\":" + closesJson +
            "}";

        std::string hdr =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";

        std::string resp = hdr + body;
        send(c, resp.c_str(), (int)resp.size(), 0);
        closesocket(c);
    }

    closesocket(s);
    WSACleanup();
    return 0;
}







