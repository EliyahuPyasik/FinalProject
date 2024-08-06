#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "serverHandler.h"
#include "implot.h"
#include <d3d9.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <atomic>


const int MAX_TICKS = 50;

std::atomic<bool> data_ready(false);
std::vector<float> exchangeRateHistoryBuffer;
std::mutex data_mutex;

// Include necessary headers for image loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "imgui_internal.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
using namespace serverHandler;

// Data
static LPDIRECT3D9              g_pD3D = nullptr;
static LPDIRECT3DDEVICE9        g_pd3dDevice = nullptr;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Currency data
struct Currency {
    std::string code;
    std::string name;
};

//std::vector<string> currencies = getCurrencyCodes();
std::vector<std::string> currencyCodes = {
    "usd", "eur", "jpy", "gbp", "aud", "cad", "chf", "cny", "sek", "nzd",
    "mxn", "sgd", "hkd", "nok", "krw", "try", "rub", "inr", "brl", "zar",
    "dkk", "pln", "thb", "myr", "huf", "czk", "ils", "php", "idr", "sar",
    "aed", "clp", "cop", "ron", "hrk", "bgn", "ars", "egp", "qar", "kwd",
    "vnd", "bhd", "pkr", "bdt", "mad", "lkr", "pen", "omr", "iqd", "ngn"
};

std::map<std::string, std::string> currencies = {
    {"usd", "United States Dollar"},
    {"eur", "Euro"},
    {"jpy", "Japanese Yen"},
    {"gbp", "British Pound Sterling"},
    {"aud", "Australian Dollar"},
    {"cad", "Canadian Dollar"},
    {"chf", "Swiss Franc"},
    {"cny", "Chinese Yuan Renminbi"},
    {"sek", "Swedish Krona"},
    {"nzd", "New Zealand Dollar"},
    {"mxn", "Mexican Peso"},
    {"sgd", "Singapore Dollar"},
    {"hkd", "Hong Kong Dollar"},
    {"nok", "Norwegian Krone"},
    {"krw", "South Korean Won"},
    {"try", "Turkish Lira"},
    {"rub", "Russian Ruble"},
    {"inr", "Indian Rupee"},
    {"brl", "Brazilian Real"},
    {"zar", "South African Rand"},
    {"dkk", "Danish Krone"},
    {"pln", "Polish Zloty"},
    {"thb", "Thai Baht"},
    {"myr", "Malaysian Ringgit"},
    {"huf", "Hungarian Forint"},
    {"czk", "Czech Koruna"},
    {"ils", "Israeli Shekel"},
    {"php", "Philippine Peso"},
    {"idr", "Indonesian Rupiah"},
    {"sar", "Saudi Riyal"},
    {"aed", "United Arab Emirates Dirham"},
    {"clp", "Chilean Peso"},
    {"cop", "Colombian Peso"},
    {"ron", "Romanian Leu"},
    {"hrk", "Croatian Kuna"},
    {"bgn", "Bulgarian Lev"},
    {"ars", "Argentine Peso"},
    {"egp", "Egyptian Pound"},
    {"qar", "Qatari Riyal"},
    {"kwd", "Kuwaiti Dinar"},
    {"vnd", "Vietnamese Dong"},
    {"bhd", "Bahraini Dinar"},
    {"pkr", "Pakistani Rupee"},
    {"bdt", "Bangladeshi Taka"},
    {"mad", "Moroccan Dirham"},
    {"lkr", "Sri Lankan Rupee"},
    {"pen", "Peruvian Sol"},
    {"omr", "Omani Rial"},
    {"iqd", "Iraqi Dinar"},
    {"ngn", "Nigerian Naira"}
};

void ImageRotated(ImTextureID tex_id, ImVec2 size, float angle) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 center = ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);

    float cos_a = cosf(angle);
    float sin_a = sinf(angle);
    ImVec2 vertices[4];
    vertices[0] = ImVec2(-0.5f * size.x, -0.5f * size.y);
    vertices[1] = ImVec2(0.5f * size.x, -0.5f * size.y);
    vertices[2] = ImVec2(0.5f * size.x, 0.5f * size.y);
    vertices[3] = ImVec2(-0.5f * size.x, 0.5f * size.y);

    for (int i = 0; i < 4; i++) {
        float x = vertices[i].x;
        float y = vertices[i].y;
        vertices[i].x = center.x + x * cos_a - y * sin_a;
        vertices[i].y = center.y + x * sin_a + y * cos_a;
    }

    ImVec2 uv0(0.0f, 0.0f);
    ImVec2 uv1(1.0f, 0.0f);
    ImVec2 uv2(1.0f, 1.0f);
    ImVec2 uv3(0.0f, 1.0f);

    window->DrawList->AddImageQuad(tex_id, vertices[0], vertices[1], vertices[2], vertices[3], uv0, uv1, uv2, uv3, IM_COL32_WHITE);
}

// Function to format the date labels
std::string FormatDate(std::time_t value) {
    std::tm tm;
    localtime_s(&tm, &value);
    char buffer[11];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
    return std::string(buffer);
}

void FetchData(int fromCurrencyIndex, int toCurrencyIndex, int selectedDurationIndex, int multiplier, const std::string& startDate) {
    std::vector<float> newExchangeRateHistory;

    int year = std::stoi(startDate.substr(0, 4));
    int month = std::stoi(startDate.substr(5, 2));
    int day = std::stoi(startDate.substr(8, 2));

    auto duration = serverHandler::getDurations()[selectedDurationIndex];
    auto duration_enum = serverHandler::duration::days;
    if (duration == "weeks") {
        duration_enum = serverHandler::duration::weeks;
    }
    else if (duration == "months") {
        duration_enum = serverHandler::duration::months;
    }
    else if (duration == "years") {
        duration_enum = serverHandler::duration::years;
    }

    auto dateRateMap = serverHandler::getInfo(year, month, day, currencyCodes[fromCurrencyIndex], currencyCodes[toCurrencyIndex], duration_enum, multiplier);

    for (const auto& entry : dateRateMap) {
        newExchangeRateHistory.push_back(entry.second);
    }

    std::lock_guard<std::mutex> lock(data_mutex);
    exchangeRateHistoryBuffer = newExchangeRateHistory;
    data_ready = true;
}

// Function to calculate start date based on duration and multiplication
std::string calculateStartDate(int selectedIndex, int multiplier) {
    using namespace std::chrono;
    auto now = system_clock::now();
    time_t today = system_clock::to_time_t(now);
    tm local_tm;
    localtime_s(&local_tm, &today);

    std::string duration = serverHandler::getDurations()[selectedIndex];

    if (duration == "days") {
        now -= hours(24 * multiplier);
    }
    else if (duration == "weeks") {
        now -= hours(24 * 7 * multiplier);
    }
    else if (duration == "months") {
        for (int i = 0; i < multiplier; ++i) {
            serverHandler::adjustMonth(local_tm.tm_year, local_tm.tm_mon, local_tm.tm_mday, 1);
        }
    }
    else if (duration == "years") {
        local_tm.tm_year -= multiplier;
    }

    auto start_date = system_clock::to_time_t(now);
    if (duration == "months" || duration == "years") {
        start_date = mktime(&local_tm);
    }

    std::ostringstream oss;
    localtime_s(&local_tm, &start_date);
    oss << std::put_time(&local_tm, "%Y-%m-%d");
    return oss.str();
}



// Function to load a texture from a file
bool LoadTextureFromFile(const char* filename, LPDIRECT3DTEXTURE9* out_texture, int* out_width, int* out_height) {
    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
    if (image_data == NULL)
        return false;

    // Create texture
    D3DLOCKED_RECT locked_rect;
    if (FAILED(g_pd3dDevice->CreateTexture(image_width, image_height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, out_texture, NULL)))
        return false;

    if (FAILED((*out_texture)->LockRect(0, &locked_rect, NULL, 0)))
        return false;

    for (int y = 0; y < image_height; y++) {
        memcpy((unsigned char*)locked_rect.pBits + locked_rect.Pitch * y, image_data + (image_width * 4) * y, image_width * 4);
    }

    (*out_texture)->UnlockRect(0);
    stbi_image_free(image_data);

    *out_width = image_width;
    *out_height = image_height;

    return true;
}

// Main code
int main(int, char**)
{
    LPDIRECT3DTEXTURE9 loadingTexture = nullptr; // Texture for the loading PNG
    bool isLoading = false; // Flag to indicate if data is being loaded
    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Currency Converter", WS_OVERLAPPEDWINDOW, 100, 100, 800, 600, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    // Load arrow texture
    LPDIRECT3DTEXTURE9 arrow_texture = NULL;
    LPDIRECT3DTEXTURE9 split_arrow_texture = NULL;
    int arrow_width = 10;
    int arrow_height = 10;
    int loading_width = 50;
    int loading_height = 50;
    //images
    bool arrow_texture_loaded = LoadTextureFromFile("./images/arrows.png", &arrow_texture, &arrow_width, &arrow_height);
    bool split_arrow_texture_loaded = LoadTextureFromFile("./images/arrow split.png", &split_arrow_texture, &arrow_width, &arrow_height);
    bool loading_texture_loaded = LoadTextureFromFile("./images/loading.png", &loadingTexture, &loading_width, &loading_height);
    //indeces of currency in vector
    int fromCurrencyIndex = 1;
    int toCurrencyIndex = 1;  
    std::string selectedDuration = "days";
    int multiplier = 1; // Multiplication
    int selectedDurationIndex = 0; // Index for "days"
    const char* duration_items[] = { "days", "weeks", "months", "years" };

    std::vector<std::string> durations = serverHandler::getDurations();
    std::vector<float> exchangeRateHistory;
    std::string startDate = calculateStartDate(selectedDurationIndex, multiplier);

    // Function to update the exchange rate history
    auto updateExchangeRateHistory = [&]() {
        int year = std::stoi(startDate.substr(0, 4));
        int month = std::stoi(startDate.substr(5, 2));
        int day = std::stoi(startDate.substr(8, 2));

        auto duration = serverHandler::getDurations()[selectedDurationIndex];
        auto duration_enum = serverHandler::duration::days;
        if (duration == "weeks") {
            duration_enum = serverHandler::duration::weeks;
        }
        else if (duration == "months") {
            duration_enum = serverHandler::duration::months;
        }
        else if (duration == "years") {
            duration_enum = serverHandler::duration::years;
        }

        auto dateRateMap = serverHandler::getInfo(year, month, day, currencyCodes[fromCurrencyIndex], currencyCodes[toCurrencyIndex], duration_enum, multiplier);

        exchangeRateHistory.clear();
        for (const auto& entry : dateRateMap) {
            exchangeRateHistory.push_back(entry.second);
        }
    };


    ImPlot::CreateContext();

    startDate = calculateStartDate(selectedDurationIndex, multiplier);
    isLoading = true;
    std::thread(FetchData, fromCurrencyIndex, toCurrencyIndex, selectedDurationIndex, multiplier, startDate).detach();

    // Main loop
    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Create our window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin(" ", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

        // Dropdown for currencies
        ImGui::SetNextItemWidth(150);
        if (ImGui::BeginCombo("##From", currencies[currencyCodes[fromCurrencyIndex]].c_str())) {
            for (int n = 0; n < currencies.size(); n++) {
                const bool is_selected = (fromCurrencyIndex == n);
                if (ImGui::Selectable(currencies[currencyCodes[n]].c_str(), is_selected))
                    fromCurrencyIndex = n;
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Arrow image
        ImGui::SameLine();
        if (arrow_texture_loaded) {
            ImVec4 tint_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Red color with full opacity
            ImGui::Image((void*)arrow_texture, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), tint_color);

        }
        else {
            ImGui::Text("->");
        }

        // Dropdown for currencies
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        if (ImGui::BeginCombo("##To", currencies[currencyCodes[toCurrencyIndex]].c_str())) {
            for (int n = 0; n < currencies.size(); n++) {
                const bool is_selected = (toCurrencyIndex == n);
                if (ImGui::Selectable(currencies[currencyCodes[n]].c_str(), is_selected))
                    toCurrencyIndex = n;
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        
        
        // Dropdown for duration
        ImGui::SetNextItemWidth(100);
        if (ImGui::BeginCombo("##Duration", durations[selectedDurationIndex].c_str())) {
            for (int i = 0; i < durations.size(); i++) {
                const bool is_selected = (selectedDurationIndex == i);
                if (ImGui::Selectable(durations[i].c_str(), is_selected))
                    selectedDurationIndex = i;
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (split_arrow_texture_loaded) {
            ImGui::Image((void*)split_arrow_texture, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1));

        }
        else {
            ImGui::Text(" X ");
        }

        ImGui::SameLine();
        // Input for multiplier
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("##Multiplier", &multiplier);

        // Button to fetch data and update the graph
        if (ImGui::Button("Update Graph")) {
            startDate = calculateStartDate(selectedDurationIndex, multiplier);
            isLoading = true;
            std::thread(FetchData, fromCurrencyIndex, toCurrencyIndex, selectedDurationIndex, multiplier, startDate).detach();
        }

        // Check if data is ready and update the graph
        if (data_ready) {
            std::lock_guard<std::mutex> lock(data_mutex);
            exchangeRateHistory = exchangeRateHistoryBuffer;
            data_ready = false;
            isLoading = false;
        }

        // Graph with ImPlot
        ImGui::Text("Exchange Rate History");
        if (!exchangeRateHistory.empty()) {
            if (ImPlot::BeginPlot("Exchange Rate History", ImVec2(-1, 300))) {
                std::vector<double> x_data(exchangeRateHistory.size());
                std::vector<double> y_data(exchangeRateHistory.begin(), exchangeRateHistory.end());

                // Generate X-axis data (dates)
                std::tm tm = {};
                std::istringstream ss(startDate);
                ss >> std::get_time(&tm, "%Y-%m-%d");
                std::time_t start_time = std::mktime(&tm);
                auto duration = serverHandler::getDurations()[selectedDurationIndex];
                int interval = (duration == "days") ? 24 * 60 * 60 :
                    (duration == "weeks") ? 7 * 24 * 60 * 60 :
                    (duration == "months") ? 30 * 24 * 60 * 60 :
                    365 * 24 * 60 * 60;

                for (size_t i = 0; i < x_data.size(); ++i) {
                    x_data[i] = start_time + i * interval;
                }

                // Adjust tick interval to ensure no more than MAX_TICKS are displayed
                int tick_interval = std::max(1, static_cast<int>(x_data.size() / MAX_TICKS));

                // Generate ticks and labels for the X-axis
                std::vector<double> ticks;
                std::vector<std::string> labels;
                for (size_t i = 0; i < x_data.size(); i += tick_interval) {
                    ticks.push_back(x_data[i]);
                    labels.push_back(FormatDate(static_cast<std::time_t>(x_data[i])));
                }

                // Convert labels to const char* array
                std::vector<const char*> labels_cstr;
                for (const auto& label : labels) {
                    labels_cstr.push_back(label.c_str());
                }

                // Configure X-axis with custom ticks and labels
                ImPlot::SetupAxis(ImAxis_X1, "Date", ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit);
                ImPlot::SetupAxisTicks(ImAxis_X1, ticks.data(), static_cast<int>(ticks.size()), labels_cstr.data());

                // Configure Y-axis for rate display
                ImPlot::SetupAxis(ImAxis_Y1, "Rate", ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit);

                // Plot the exchange rate line
                ImPlot::PlotLine("Exchange Rate", x_data.data(), y_data.data(), y_data.size());

                ImPlot::EndPlot();
            }
        }
        else {
            ImGui::Text("No data available");
        }

        // Show loading PNG if data is being fetched
        if (isLoading && loadingTexture != nullptr) {
            ImVec2 center = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::Begin("Loading", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
            static float rotation = 0.0f;
            rotation += ImGui::GetIO().DeltaTime * 3.0f; // Adjust rotation speed as needed
            ImageRotated((void*)loadingTexture, ImVec2(100, 100), rotation);
            ImGui::End();
        }

        ImGui::End();

        // Rendering
        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(255.0f), (int)(255.0f), (int)(255.0f), (int)(255.0f));
        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0) {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT result = g_pd3dDevice->Present(NULL, NULL, NULL, NULL);

        if (result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
            ResetDevice();
    }

    // Cleanup
    ImPlot::DestroyContext(); // Added ImPlot context destruction
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions
bool CreateDeviceD3D(HWND hWnd) {
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
        return false;

    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

void CleanupDeviceD3D() {
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
}

void ResetDevice() {
    // Invalidate device objects
    ImGui_ImplDX9_InvalidateDeviceObjects();

    // Check if the device can be reset
    HRESULT hr = g_pd3dDevice->TestCooperativeLevel();
    if (hr == D3DERR_DEVICELOST) {
        // Device is lost and cannot be reset yet
        return;
    }
    if (hr == D3DERR_DEVICENOTRESET) {
        // Device is ready to be reset
        hr = g_pd3dDevice->Reset(&g_d3dpp);
        if (FAILED(hr)) {
            // Reset failed, handle error
            IM_ASSERT(0); // Trigger assertion if reset fails
            return;
        }
    }

    // Recreate device objects
    ImGui_ImplDX9_CreateDeviceObjects();
}



LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
            g_d3dpp.BackBufferWidth = LOWORD(lParam);
            g_d3dpp.BackBufferHeight = HIWORD(lParam);
            ResetDevice();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
