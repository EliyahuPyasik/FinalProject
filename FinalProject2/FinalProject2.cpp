#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "serverHandler.h"
#include <d3d9.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

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

std::vector<Currency> currencies = {
        {"USD", "US Dollar"},
        {"EUR", "Euro"},
        {"GBP", "British Pound"},
        {"JPY", "Japanese Yen"},
        {"AUD", "Australian Dollar"},
        {"CAD", "Canadian Dollar"},
        {"CHF", "Swiss Franc"},
        {"CNY", "Chinese Yuan"},
        {"ILS", "Israeli Shekel"},
        {"INR", "Indian Rupee"},
        {"RUB", "Russian Ruble"},
        {"BRL", "Brazilian Real"},
        {"ZAR", "South African Rand"},
        {"SGD", "Singapore Dollar"},
        {"HKD", "Hong Kong Dollar"},
        {"KRW", "South Korean Won"},
        {"MXN", "Mexican Peso"},
        {"NOK", "Norwegian Krone"},
        {"SEK", "Swedish Krona"},
        {"NZD", "New Zealand Dollar"},
        {"TRY", "Turkish Lira"},
        {"THB", "Thai Baht"},
        {"MYR", "Malaysian Ringgit"},
        {"IDR", "Indonesian Rupiah"},
        {"PHP", "Philippine Peso"},
        {"CZK", "Czech Koruna"},
        {"HUF", "Hungarian Forint"},
        {"PLN", "Polish Zloty"},
        {"DKK", "Danish Krone"},
        {"RON", "Romanian Leu"},
};

float getExchangeRate(const std::string& from, const std::string& to) {
    std::string key = from + to;

    /*if (exchangeRates.find(key) != exchangeRates.end()) {
        return exchangeRates[key];
    }*/
    return 1.0f; // Default to 1:1 if not found
}

// Dummy data for exchange rate history (replace with real data in a production app)
std::vector<float> getDummyExchangeRateHistory() {
    return std::vector<float>{1.0f, 1.1f, 1.2f, 1.1f, 1.0f, 0.9f, 0.8f, 0.85f, 0.9f, 0.95f, 1.0f, 1.05f, 1.0f, 1.1f, 1.2f, 1.1f, 1.0f, 0.9f, 0.8f, 0.85f, 0.9f, 0.95f, 1.0f, 1.05f};
}

// Main code
int main(int, char**)
{
    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Currency Converter", WS_OVERLAPPEDWINDOW, 100, 100, 400, 600, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
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

    // Our state
    float amount = 1.0f;
    int fromCurrencyIndex = 0; // USD
    int toCurrencyIndex = 8;   // ILS
    float result = 0.0f;
    char filterText[64] = "";

    // Initial conversion
    result = amount * getExchangeRate(currencies[fromCurrencyIndex].code, currencies[toCurrencyIndex].code);

    // Main loop
    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
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
        ImGui::Begin("Currency Converter", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

        ImGui::Text("Amount");
        ImGui::SameLine(100);
        ImGui::SetNextItemWidth(200);
        if (ImGui::InputFloat("##Amount", &amount, 0.0f, 0.0f, "%.2f"))
        {
            // Update result whenever amount changes
            float rate = getExchangeRate(currencies[fromCurrencyIndex].code, currencies[toCurrencyIndex].code);
            result = amount * rate;
        }

        ImGui::Text("From");
        ImGui::SameLine(100);
        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("##From", currencies[fromCurrencyIndex].code.c_str()))
        {
            ImGui::InputText("##FilterFrom", filterText, sizeof(filterText));
            std::string filterStr = filterText;
            std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::toupper);

            for (int i = 0; i < currencies.size(); i++)
            {
                if (filterStr.empty() || currencies[i].code.find(filterStr) != std::string::npos)
                {
                    bool is_selected = (fromCurrencyIndex == i);
                    if (ImGui::Selectable(currencies[i].code.c_str(), is_selected))
                        fromCurrencyIndex = i;
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Text("To");
        ImGui::SameLine(100);
        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("##To", currencies[toCurrencyIndex].code.c_str()))
        {
            ImGui::InputText("##FilterTo", filterText, sizeof(filterText));
            std::string filterStr = filterText;
            std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::toupper);

            for (int i = 0; i < currencies.size(); i++)
            {
                if (filterStr.empty() || currencies[i].code.find(filterStr) != std::string::npos)
                {
                    bool is_selected = (toCurrencyIndex == i);
                    if (ImGui::Selectable(currencies[i].code.c_str(), is_selected))
                        toCurrencyIndex = i;
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Switch"))
        {
            std::swap(fromCurrencyIndex, toCurrencyIndex);
            float rate = getExchangeRate(currencies[fromCurrencyIndex].code, currencies[toCurrencyIndex].code);
            result = amount * rate;
        }

        ImGui::SameLine();

        if (ImGui::Button("Convert"))
        {
            float rate = getExchangeRate(currencies[fromCurrencyIndex].code, currencies[toCurrencyIndex].code);
            result = amount * rate;
        }

        ImGui::Text("%.2f %s Equals %.2f %s",
            amount,
            currencies[fromCurrencyIndex].name.c_str(),
            result,
            currencies[toCurrencyIndex].name.c_str());

        ImGui::Separator();
        ImGui::Text("Exchange Rate History");
        std::vector<float> dummyRates = getDummyExchangeRateHistory();
        ImGui::PlotLines("##RateHistory", dummyRates.data(), dummyRates.size(), 0, nullptr, FLT_MIN, FLT_MAX, ImVec2(0, 80));

        ImGui::End();

        // Rendering
        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA(240, 240, 240, 255);
        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT result = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
        if (result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
            ResetDevice();
    }

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return false;

    // Create the D3DDevice
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

void CleanupDeviceD3D()
{
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
}

void ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            g_d3dpp.BackBufferWidth = LOWORD(lParam);
            g_d3dpp.BackBufferHeight = HIWORD(lParam);
            ResetDevice();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}