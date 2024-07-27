#include <windows.h>
#include <wininet.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <regex>
#include <json/json.h>

#pragma comment(lib, "wininet.lib")

#define ID_EDIT 1
#define ID_BUTTON 2

const char* API_KEY = "YOUR_API_KEY_HERE";
const char g_szClassName[] = "YouTubePlaylistDurationCalculator";

// Function prototypes
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
std::string HttpRequest(const std::string& url);
std::vector<std::string> GetPlaylistItemIds(const std::string& playlistId);
int GetTotalDuration(const std::vector<std::string>& videoIds);
int ParseDuration(const std::string& duration);

// Global variables
HWND g_hEdit, g_hButton, g_hResult;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc;
    HWND hwnd;
    MSG Msg;

    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = 0;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = g_szClassName;
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

    if(!RegisterClassEx(&wc))
    {
        MessageBox(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        g_szClassName,
        "YouTube Playlist Duration Calculator",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
        NULL, NULL, hInstance, NULL);

    if(hwnd == NULL)
    {
        MessageBox(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while(GetMessage(&Msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }
    return Msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
        case WM_CREATE:
            g_hEdit = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER, 
                10, 10, 360, 20, hwnd, (HMENU)ID_EDIT, NULL, NULL);
            g_hButton = CreateWindow("BUTTON", "Calculate Duration", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 
                10, 40, 120, 30, hwnd, (HMENU)ID_BUTTON, NULL, NULL);
            g_hResult = CreateWindow("STATIC", "", WS_VISIBLE | WS_CHILD, 
                10, 80, 360, 20, hwnd, NULL, NULL, NULL);
            break;
        case WM_COMMAND:
            if(LOWORD(wParam) == ID_BUTTON)
            {
                char buffer[1024];
                GetWindowText(g_hEdit, buffer, 1024);
                std::string url(buffer);

                size_t pos = url.find("list=");
                if (pos == std::string::npos)
                {
                    SetWindowText(g_hResult, "Invalid playlist URL");
                    break;
                }

                std::string playlistId = url.substr(pos + 5);
                pos = playlistId.find('&');
                if (pos != std::string::npos)
                    playlistId = playlistId.substr(0, pos);

                try
                {
                    std::vector<std::string> videoIds = GetPlaylistItemIds(playlistId);
                    int totalSeconds = GetTotalDuration(videoIds);
                    int hours = totalSeconds / 3600;
                    int minutes = (totalSeconds % 3600) / 60;
                    int seconds = totalSeconds % 60;

                    char result[100];
                    sprintf(result, "Total duration: %02d:%02d:%02d", hours, minutes, seconds);
                    SetWindowText(g_hResult, result);
                }
                catch (const std::exception& e)
                {
                    SetWindowText(g_hResult, e.what());
                }
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

std::string HttpRequest(const std::string& url)
{
    HINTERNET hInternet = InternetOpen("YouTubePlaylistDurationCalculator", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) throw std::runtime_error("Failed to open internet connection");

    HINTERNET hConnect = InternetOpenUrl(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hConnect)
    {
        InternetCloseHandle(hInternet);
        throw std::runtime_error("Failed to open URL");
    }

    std::string response;
    char buffer[1024];
    DWORD bytesRead;
    while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0)
    {
        response.append(buffer, bytesRead);
    }

    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    return response;
}

std::vector<std::string> GetPlaylistItemIds(const std::string& playlistId)
{
    std::vector<std::string> videoIds;
    std::string pageToken;

    do
    {
        std::string url = "https://www.googleapis.com/youtube/v3/playlistItems?part=contentDetails&maxResults=50&playlistId=" + 
                          playlistId + "&key=" + API_KEY + "&pageToken=" + pageToken;
        std::string response = HttpRequest(url);

        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(response, root)) throw std::runtime_error("Failed to parse JSON");

        const Json::Value& items = root["items"];
        for (const auto& item : items)
        {
            videoIds.push_back(item["contentDetails"]["videoId"].asString());
        }

        pageToken = root["nextPageToken"].asString();
    } while (!pageToken.empty());

    return videoIds;
}

int GetTotalDuration(const std::vector<std::string>& videoIds)
{
    int totalSeconds = 0;
    for (size_t i = 0; i < videoIds.size(); i += 50)
    {
        std::vector<std::string> batch(videoIds.begin() + i, videoIds.begin() + std::min(i + 50, videoIds.size()));
        std::string ids = std::accumulate(batch.begin(), batch.end(), std::string(),
            [](const std::string& a, const std::string& b) { return a.empty() ? b : a + ',' + b; });

        std::string url = "https://www.googleapis.com/youtube/v3/videos?part=contentDetails&id=" + ids + "&key=" + API_KEY;
        std::string response = HttpRequest(url);

        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(response, root)) throw std::runtime_error("Failed to parse JSON");

        const Json::Value& items = root["items"];
        for (const auto& item : items)
        {
            std::string duration = item["contentDetails"]["duration"].asString();
            totalSeconds += ParseDuration(duration);
        }
    }

    return totalSeconds;
}

int ParseDuration(const std::string& duration)
{
    std::regex re("PT(?:(\\d+)H)?(?:(\\d+)M)?(?:(\\d+)S)?");
    std::smatch match;
    if (std::regex_match(duration, match, re))
    {
        int hours = match[1].matched ? std::stoi(match[1].str()) : 0;
        int minutes = match[2].matched ? std::stoi(match[2].str()) : 0;
        int seconds = match[3].matched ? std::stoi(match[3].str()) : 0;
        return hours * 3600 + minutes * 60 + seconds;
    }
    return 0;
}
