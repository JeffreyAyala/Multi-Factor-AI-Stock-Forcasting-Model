#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <iostream>
#include <vector>
#include <cstdlib>
#include "PolygonAPI.hpp"
#include "Predictor.hpp"

#pragma comment(lib, "ws2_32.lib")

static std::string urlDecode(const std::string& src) {
    std::string out;
    for (size_t i = 0; i < src.size(); ++i) {
        if (src[i] == '%' && i + 2 < src.size()) {
            std::string hex = src.substr(i + 1, 2);
            char ch = static_cast<char>(strtol(hex.c_str(), nullptr, 16));
            out.push_back(ch);
            i += 2;
        }
        else if (src[i] == '+') {
            out.push_back(' ');
        }
        else {
            out.push_back(src[i]);
        }
    }
    return out;
}

int main() {
    PolygonAPI api("iEt9sFSDGEvS1qdxzKn9ggSLCyBBpG5Q");
    Predictor predictor(&api);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cout << "WSAStartup failed\n";
        return 1;
    }

    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) {
        std::cout << "Socket failed\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port = htons(5000);

    if (bind(s, (sockaddr*)&srv, sizeof(srv)) == SOCKET_ERROR) {
        std::cout << "Bind failed\n";
        closesocket(s);
        WSACleanup();
        return 1;
    }

    listen(s, 5);
    std::cout << "Server running at http://localhost:5000\n";

    while (true) {
        SOCKET c = accept(s, nullptr, nullptr);
        if (c == INVALID_SOCKET) continue;

        char buf[4096] = { 0 };
        int recvd = recv(c, buf, sizeof(buf) - 1, 0);
        if (recvd <= 0) {
            closesocket(c);
            continue;
        }

        std::string req(buf);

        std::string ticker = "AAPL";
        int aheadMinutes = 1;   // default horizon = 1 minute

        size_t getPos = req.find("GET ");
        if (getPos != std::string::npos) {
            size_t pathStart = getPos + 4;
            size_t pathEnd = req.find(' ', pathStart);
            std::string path = req.substr(pathStart, pathEnd - pathStart);

            // parse ticker=
            size_t qPos = path.find("ticker=");
            if (qPos != std::string::npos) {
                size_t valStart = qPos + 7;
                size_t valEnd = path.find_first_of("& ", valStart);
                ticker = urlDecode(path.substr(valStart, valEnd - valStart));
            }

            // parse ahead=
            size_t aPos = path.find("ahead=");
            if (aPos != std::string::npos) {
                size_t valStart = aPos + 6;
                size_t valEnd = path.find_first_of("& ", valStart);
                std::string aheadStr = path.substr(valStart, valEnd - valStart);
                try {
                    int v = std::stoi(aheadStr);
                    if (v >= 1 && v <= 120) aheadMinutes = v;
                }
                catch (...) {
                    // ignore invalid input and keep default
                }
            }
        }

        Indicators ind;
        std::vector<double> closes;
        double current = api.getLastPrice(ticker);
        double prediction = predictor.predictNextPrice(ticker, ind, closes, aheadMinutes);

        std::string closesJson = "[";
        for (size_t i = 0; i < closes.size(); ++i) {
            closesJson += std::to_string(closes[i]);
            if (i + 1 < closes.size()) closesJson += ",";
        }
        closesJson += "]";

        std::string body = "{"
            "\"ticker\":\"" + ticker + "\","
            "\"ahead\":" + std::to_string(aheadMinutes) + ","
            "\"current\":" + std::to_string(current) + ","
            "\"prediction\":" + std::to_string(prediction) + ","
            "\"sma20\":" + std::to_string(ind.sma20) + ","
            "\"ema20\":" + std::to_string(ind.ema20) + ","
            "\"rsi14\":" + std::to_string(ind.rsi14) + ","
            "\"closes\":" + closesJson +
            "}";

        std::string header =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";

        std::string resp = header + body;
        send(c, resp.c_str(), (int)resp.size(), 0);
        closesocket(c);
    }

    closesocket(s);
    WSACleanup();
    return 0;
}
