#include "DarkPlayer.h"

#include "vs.h"
#include "vs2.h"
#include "ps.h"
#include "ps2.h"
#include "ps3.h"

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

#define TICKRATE 20
float remainingTick = 0.0f;
LARGE_INTEGER freq, t0, t1;
double countsPerTick, accum;
float frameDeltaSec;

DWORD useAccent;
DWORD accentColor;
bool isFocused = false;
void getAccent(void) {

    HKEY key = 0;
    LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\DWM", 0, KEY_READ, &key);
    assert(status == ERROR_SUCCESS);

    DWORD size = sizeof(DWORD);

    DWORD frame_color = 0;
    status = RegGetValueW(key, 0, L"ColorizationColor", RRF_RT_REG_DWORD, 0, &frame_color, &size);

    DWORD balance = 0;
    status = RegGetValueW(key, 0, L"ColorizationColorBalance", RRF_RT_REG_DWORD, 0, &balance, &size);

    status = RegGetValueW(key, 0, L"ColorPrevalence", RRF_RT_REG_DWORD, 0, &useAccent, &size);
    assert(status == ERROR_SUCCESS);

    status = RegCloseKey(key);
    assert(status == ERROR_SUCCESS);

    if (useAccent) {

        DWORD frame_blend_color = 0x00d9d9d9;

        float factor_a = ((float)balance) / 100.0f;
        float factor_b = ((float)(100 - balance)) / 100.0f;

        float a_r = (float)((frame_color >> 16) & 0xff);
        float a_g = (float)((frame_color >> 8) & 0xff);
        float a_b = (float)(frame_color & 0xff);

        float b_r = (float)((frame_blend_color >> 16) & 0xff);
        float b_g = (float)((frame_blend_color >> 8) & 0xff);
        float b_b = (float)(frame_blend_color & 0xff);

        int r = (int)roundf(a_r * factor_a + b_r * factor_b);
        int g = (int)roundf(a_g * factor_a + b_g * factor_b);
        int b = (int)roundf(a_b * factor_a + b_b * factor_b);

        accentColor = (b << 16) | (g << 8) | (r);

    }
    else {

        accentColor = 0xb2323232;
    }
}

int prevmousex, prevmousey, mousex, mousey;
int btnid, hoveredid, ldownid = -1;
int actiontype;
#define ACTION_MOVE 0
#define ACTION_LDOWN 1
#define ACTION_LUP 2

bool playing = false;
float progress = 0.10f;
double elapsedSec = 0.0;
double currentSongDuration = 0.0;
#define SWINGOUT_TICKS 10
float prevpanelx = PANEL_LEFT_STOP;
float curpanelx = PANEL_LEFT_STOP;
float panelx = PANEL_LEFT_STOP;
float prevpanely = 70.0f;
float curpanely = 70.0f;
float panely = 70.0f;
float panelyvel = 0.0f;
int panelyoverride = 0;
int panelticks = 0;
int nAlbums;
float selxvel = 0.0f;
float prevselx = 0.0f;
float curselx = 0.0f;
float selx = 0.0f;
float selyvel = 0.0f;
float prevsely = 0.0f;
float cursely = 0.0f;
float sely = 0.0f;
int selAlbum = 0;
bool selActive = false;

enum State {
    DEFAULT,
    PANEL_SWING_OUT,
    PANEL_SWING_IN,
    PANEL
};

enum State state = DEFAULT;

float dist(float ax, float ay, float bx, float by) {
    float abx = bx - ax;
    float aby = by - ay;
    return sqrtf(abx * abx + aby * aby);
}

float lineSegDistToPoint(float ax, float ay, float bx, float by, float cx, float cy) {
    if (ax == bx && ay == by) {
        return dist(ax, ay, cx, cy);
    }

    float acx = cx - ax;
    float acy = cy - ay;

    float abx = bx - ax;
    float aby = by - ay;

    float k = (acx * abx + acy * aby) / (abx * abx + aby * aby);

    float dx = ax + k * abx;
    float dy = ay + k * aby;

    float adx = dx - ax;
    float ady = dy - ay;

    float p = fabsf(abx) > fabsf(aby) ? adx / abx : ady / aby;

    if (p <= 0.0) {
        return dist(cx, cy, ax, ay);
    }
    else if (p >= 1.0) {
        return dist(cx, cy, bx, by);
    }

    return dist(cx, cy, dx, dy);
}

bool button(float x, float y, float radius) {
    if (lineSegDistToPoint(prevmousex, prevmousey, mousex, mousey, x, y) <= radius) {
        hoveredid = btnid;
        switch (actiontype) {
        case ACTION_MOVE:
            break;
        case ACTION_LDOWN:
            ldownid = btnid;
            break;
        case ACTION_LUP:
            if (ldownid == btnid) return true;
            break;
        }
    }
    btnid++;
    return false;
}

void doButtons(LPARAM lparam, int action) {
    prevmousex = mousex;
    prevmousey = mousey;
    mousex = GET_X_LPARAM(lparam);
    mousey = GET_Y_LPARAM(lparam);
    actiontype = action;

    btnid = 0;
    hoveredid = -1;

    if (state == DEFAULT) {
        if (button(PLAYER_WIDTH / 2, PLAYER_HEIGHT - 75 * SCALE, 30 + 10)) {
            printf("play/pause\n");
            if (playing) pause();
            else play();
        }
        if (button(56 * SCALE, PLAYER_HEIGHT - 75 * SCALE, 28 + 6)) {
            printf("skip backward\n");
        }
        if (button(PLAYER_WIDTH - 56 * SCALE, PLAYER_HEIGHT - 75 * SCALE, 28 + 6)) {
            printf("skip forward\n");
        }
        if (button(PLAYER_WIDTH - 35 * SCALE, 35 * SCALE, 15 + 6)) {
            printf("close\n");
            PostQuitMessage(0);
        }
        if (button(35 * SCALE, 35 * SCALE, 15 + 6)) {
            printf("panel out\n");
            state = PANEL_SWING_OUT;
        }
    }
    else if (state == PANEL) {
        if (button(PLAYER_WIDTH - 35 * SCALE, 35 * SCALE, 15 + 6)) {
            printf("panel in\n");
            state = PANEL_SWING_IN;
        }
    }
}

float clamp(float x, float lowerlimit = 0.0f, float upperlimit = 1.0f) {
    if (x < lowerlimit) return lowerlimit;
    if (x > upperlimit) return upperlimit;
    return x;
}

float smootherstep(float edge0, float edge1, float x) {
    // Scale, and clamp x to 0..1 range
    x = clamp((x - edge0) / (edge1 - edge0));

    return x * x * x * (x * (6.0f * x - 15.0f) + 10.0f);
}

void tick() {
    prevpanelx = curpanelx;
    if (state == PANEL_SWING_OUT) {
        panelticks++;
        curpanelx = PANEL_LEFT_STOP + (PANEL_RIGHT_STOP - PANEL_LEFT_STOP) * smootherstep(0.0f, 1.0f, ((float)panelticks / SWINGOUT_TICKS));
        if (panelticks == SWINGOUT_TICKS) {
            state = PANEL;
        }
    }
    else if (state == PANEL_SWING_IN) {
        panelticks--;
        curpanelx = PANEL_LEFT_STOP + (PANEL_RIGHT_STOP - PANEL_LEFT_STOP) * smootherstep(0.0f, 1.0f, ((float)panelticks / SWINGOUT_TICKS));
        if (panelticks == 0) {
            state = DEFAULT;
        }
    }
    prevpanely = curpanely;
    float low = 70.0f - (nAlbums-6) * 2 * ALBUM_HEIGHT;
#define OVERRIDE_EPSILON 5.0f
    if ((curpanely > 70.0f && !(panelyoverride < 0 && panelyvel <= -OVERRIDE_EPSILON)) || 
        (curpanely < low && !(panelyoverride > 0 && panelyvel >= OVERRIDE_EPSILON))) {
        float dist = (curpanely > 70.0f ? 70.0f : low) - curpanely;
        int dir = dist < 0 ? -1 : dist == 0 ? 0 : 1;
        if (dist < 0) dist = -dist;
        if (dist > 256) {}
#define HOLD_FORCE_MULTIPLIER 0.2
#define DAMPING 0.5
        panelyvel += dist * HOLD_FORCE_MULTIPLIER * dir - DAMPING * panelyvel;
    }
    panelyvel -= panelyvel * 0.2f;
    if (fabsf(panelyvel) <= 1.0f) panelyvel = 0.0f;
    curpanely += panelyvel;

    prevselx = curselx;
    prevsely = cursely;
    float target[2] = { selActive ? 132.0f : -140.0f, curpanely + selAlbum * 2 * ALBUM_HEIGHT };
    float dist[2] = { target[0] - curselx, target[1] - cursely };
    int dir[2] = { dist[0] < 0 ? -1 : dist[0] == 0 ? 0 : 1, dist[1] < 0 ? -1 : dist[1] == 0 ? 0 : 1 };
    if (dist[0] < 0) dist[0] = -dist[0];
    if (dist[1] < 0) dist[1] = -dist[1];
    if (dist[0] > 256) {}
    if (dist[1] > 256) {}
    #define HOLD_FORCE_MULTIPLIER 0.4
    #define DAMPING 1.0
    selxvel += dist[0] * HOLD_FORCE_MULTIPLIER * 2.0f * dir[0] - DAMPING * selxvel;
    selyvel += dist[1] * HOLD_FORCE_MULTIPLIER * dir[1] - DAMPING * selyvel;
    curselx += selxvel;
    cursely += selyvel;
}

void tickloop() {
    while (accum > countsPerTick) {
        tick();
        accum -= countsPerTick;
    }
    remainingTick = accum / countsPerTick;

    panelx = prevpanelx + (curpanelx - prevpanelx) * remainingTick;
    panely = prevpanely + (curpanely - prevpanely) * remainingTick;

    selx = prevselx + (curselx - prevselx) * remainingTick;
    sely = prevsely + (cursely - prevsely) * remainingTick;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;
    switch (msg)
    {
    case WM_NCCALCSIZE: {
        if (!wparam) return DefWindowProc(hwnd, msg, wparam, lparam);
        UINT dpi = GetDpiForWindow(hwnd);

        int frame_x = GetSystemMetricsForDpi(SM_CXFRAME, dpi);
        int frame_y = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
        int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);

        NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)lparam;
        RECT* requested_client_rect = params->rgrc;

        requested_client_rect->right -= frame_x + padding;
        requested_client_rect->left += frame_x + padding;
        requested_client_rect->bottom -= frame_y + padding;

        return 0;
    }
    case WM_NCHITTEST: {
        // Let the default procedure handle resizing areas
        LRESULT hit = DefWindowProc(hwnd, msg, wparam, lparam);
        switch (hit) {
        case HTNOWHERE:
        case HTRIGHT:
        case HTLEFT:
        case HTTOPLEFT:
        case HTTOP:
        case HTTOPRIGHT:
        case HTBOTTOMRIGHT:
        case HTBOTTOM:
        case HTBOTTOMLEFT: {
            return hit;
        }
        }

        // Looks like adjustment happening in NCCALCSIZE is messing with the detection
        // of the top hit area so manually fixing that.
        UINT dpi = GetDpiForWindow(hwnd);
        int frame_y = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
        int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
        POINT cursor_point = { 0 };
        cursor_point.x = GET_X_LPARAM(lparam);
        cursor_point.y = GET_Y_LPARAM(lparam);
        ScreenToClient(hwnd, &cursor_point);

        // Since we are drawing our own caption, this needs to be a custom test
        if (cursor_point.y < 31 && hoveredid < 0) {
            return HTCAPTION;
        }

        return HTCLIENT;
    }
    case WM_CREATE: {
        // Inform the application of the frame change to force redrawing with the new
        // client area that is extended into the title bar
        SetWindowPos(
            hwnd, NULL,
            0, 0, 0, 0,
            SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
        );
        break;
    }
    case WM_MOUSEWHEEL:
    {
        short zDelta = GET_WHEEL_DELTA_WPARAM(wparam);
        panelyvel += 0.1f * zDelta;

        if (zDelta > 0)
        {
            panelyoverride = 1;
        }
        else if (zDelta < 0)
        {
            panelyoverride = -1;
        }
        break;
    }
    case WM_MOUSEMOVE: {
        doButtons(lparam, ACTION_MOVE);
        break;
    }
    case WM_LBUTTONDOWN: {
        doButtons(lparam, ACTION_LDOWN);
        break;
    }
    case WM_LBUTTONUP: {
        doButtons(lparam, ACTION_LUP);
        ldownid = -1;
        break;
    }
    case WM_SETCURSOR:
    {
        // Check if the cursor is over the client area of the window
        if (hoveredid >= 0)
        {
            // Load the hand cursor
            HCURSOR hHandCursor = LoadCursor(NULL, IDC_HAND);
            // Set the cursor to the hand cursor
            SetCursor(hHandCursor);
            return TRUE; // Indicate that the message has been handled
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    case WM_SETFOCUS: {
        isFocused = true;
    } break;

    case WM_KILLFOCUS: {
        isFocused = false;
    } break;
    case WM_DWMCOLORIZATIONCOLORCHANGED: {
        getAccent();
        break;
    }
    case WM_KEYDOWN:
    {
        if (wparam == VK_ESCAPE)
            DestroyWindow(hwnd);
        break;
    }
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        break;
    }
    default:
        result = DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    return result;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
#if _DEBUG
    if (GetConsoleWindow() == NULL) {

        // Allocate a new console for this process
        if (!AllocConsole()) {
            MessageBox(NULL, L"Failed to allocate console!", L"Error", MB_ICONERROR);
        }

        // Redirect standard C I/O (printf)
        FILE* p_stdout;
        FILE* p_stdin;
        FILE* p_stderr;

        // Use freopen_s to reassign the standard streams
        // "CONOUT$" is a special name for the console output device
        if (freopen_s(&p_stdout, "CONOUT$", "w", stdout) != 0) {
            MessageBox(NULL, L"Failed to redirect stdout!", L"Error", MB_ICONERROR);
        }
        if (freopen_s(&p_stdin, "CONIN$", "r", stdin) != 0) {
            MessageBox(NULL, L"Failed to redirect stdin!", L"Error", MB_ICONERROR);
        }
        if (freopen_s(&p_stderr, "CONOUT$", "w", stderr) != 0) {
            MessageBox(NULL, L"Failed to redirect stderr!", L"Error", MB_ICONERROR);
        }

    }
#endif

    CoInitialize(0);

    init_image_loader();

    init_audio();

    getAccent();

    std::map<std::wstring, Album> albums = iterateAlbums();
    std::vector<std::wstring> album_keys;
    for (const auto& pair : albums) {
        album_keys.push_back(pair.first);
    }
    nAlbums = album_keys.size();

    loadSong(albums[album_keys[6]].songs[17].path);

    play();
    
    // Open a window
    HWND hwnd;
    {
        WNDCLASSEXW winClass = {};
        winClass.cbSize = sizeof(WNDCLASSEXW);
        winClass.style = CS_HREDRAW | CS_VREDRAW;
        winClass.lpfnWndProc = &WndProc;
        winClass.hInstance = hInstance;
        winClass.hIcon = LoadIconW(0, MAKEINTRESOURCE(IDI_DARKPLAYER));
        winClass.hCursor = LoadCursorW(0, IDC_ARROW);
        winClass.lpszClassName = L"DarkPlayer";
        winClass.hIconSm = LoadIconW(0, MAKEINTRESOURCE(IDI_DARKPLAYER));
        winClass.hbrBackground = CreateSolidBrush(RGB(37,40,44));

        if (!RegisterClassExW(&winClass)) {
            MessageBoxA(0, "RegisterClassEx failed", "Fatal Error", MB_OK);
            return GetLastError();
        }

        /*
        RECT initialRect = { 0, 0, PLAYER_WIDTH, PLAYER_HEIGHT };
        AdjustWindowRectEx(&initialRect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_OVERLAPPEDWINDOW);
        LONG initialWidth = initialRect.right - initialRect.left;
        LONG initialHeight = initialRect.bottom - initialRect.top;
        */

        hwnd = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW,
            winClass.lpszClassName,
            winClass.lpszClassName,
            WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT,
            PLAYER_WIDTH+16,
            PLAYER_HEIGHT+9,
            0, 0, hInstance, 0);

        if (!hwnd) {
            MessageBoxA(0, "CreateWindowEx failed", "Fatal Error", MB_OK);
            return GetLastError();
        }
    }

    // Create D3D11 Device and Context
    ID3D11Device1* d3d11Device;
    ID3D11DeviceContext1* d3d11DeviceContext;
    {
        ID3D11Device* baseDevice;
        ID3D11DeviceContext* baseDeviceContext;
        D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(DEBUG_BUILD)
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        HRESULT hResult = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE,
            0, creationFlags,
            featureLevels, ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION, &baseDevice,
            0, &baseDeviceContext);
        if (FAILED(hResult)) {
            MessageBoxA(0, "D3D11CreateDevice() failed", "Fatal Error", MB_OK);
            return GetLastError();
        }

        // Get 1.1 interface of D3D11 Device and Context
        hResult = baseDevice->QueryInterface(__uuidof(ID3D11Device1), (void**)&d3d11Device);
        assert(SUCCEEDED(hResult));
        baseDevice->Release();

        hResult = baseDeviceContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)&d3d11DeviceContext);
        assert(SUCCEEDED(hResult));
        baseDeviceContext->Release();
    }

#ifdef DEBUG_BUILD
    // Set up debug layer to break on D3D11 errors
    ID3D11Debug* d3dDebug = nullptr;
    d3d11Device->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3dDebug);
    if (d3dDebug)
    {
        ID3D11InfoQueue* d3dInfoQueue = nullptr;
        if (SUCCEEDED(d3dDebug->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&d3dInfoQueue)))
        {
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
            d3dInfoQueue->Release();
        }
        d3dDebug->Release();
    }
#endif

    // Create Swap Chain
    IDXGISwapChain1* d3d11SwapChain;
    {
        // Get DXGI Factory (needed to create Swap Chain)
        IDXGIFactory2* dxgiFactory;
        {
            IDXGIDevice1* dxgiDevice;
            HRESULT hResult = d3d11Device->QueryInterface(__uuidof(IDXGIDevice1), (void**)&dxgiDevice);
            assert(SUCCEEDED(hResult));

            IDXGIAdapter* dxgiAdapter;
            hResult = dxgiDevice->GetAdapter(&dxgiAdapter);
            assert(SUCCEEDED(hResult));
            dxgiDevice->Release();

            DXGI_ADAPTER_DESC adapterDesc;
            dxgiAdapter->GetDesc(&adapterDesc);

            OutputDebugStringA("Graphics Device: ");
            OutputDebugStringW(adapterDesc.Description);

            hResult = dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&dxgiFactory);
            assert(SUCCEEDED(hResult));
            dxgiAdapter->Release();
        }

        DXGI_SWAP_CHAIN_DESC1 d3d11SwapChainDesc = {};
        d3d11SwapChainDesc.Width = 0; // use window width
        d3d11SwapChainDesc.Height = 0; // use window height
        d3d11SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        d3d11SwapChainDesc.SampleDesc.Count = 1;
        d3d11SwapChainDesc.SampleDesc.Quality = 0;
        d3d11SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        d3d11SwapChainDesc.BufferCount = 2;
        d3d11SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        d3d11SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        d3d11SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        d3d11SwapChainDesc.Flags = 0;

        HRESULT hResult = dxgiFactory->CreateSwapChainForHwnd(d3d11Device, hwnd, &d3d11SwapChainDesc, 0, 0, &d3d11SwapChain);
        assert(SUCCEEDED(hResult));

        dxgiFactory->Release();
    }

    // Create Framebuffer Render Target
    ID3D11RenderTargetView* d3d11FrameBufferView;
    {
        ID3D11Texture2D* d3d11FrameBuffer;
        HRESULT hResult = d3d11SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&d3d11FrameBuffer);
        assert(SUCCEEDED(hResult));

        hResult = d3d11Device->CreateRenderTargetView(d3d11FrameBuffer, 0, &d3d11FrameBufferView);
        assert(SUCCEEDED(hResult));
        d3d11FrameBuffer->Release();
    }

    // Create Vertex Shader
    ID3D11VertexShader* vertexShader;
    {
        HRESULT hResult = d3d11Device->CreateVertexShader(g_vs_main, sizeof(g_vs_main), nullptr, &vertexShader);
        assert(SUCCEEDED(hResult));
    }
    ID3D11VertexShader* vertexShader2;
    {
        HRESULT hResult = d3d11Device->CreateVertexShader(g_vs2_main, sizeof(g_vs2_main), nullptr, &vertexShader2);
        assert(SUCCEEDED(hResult));
    }

    // Create Pixel Shader
    ID3D11PixelShader* pixelShader;
    {
        HRESULT hResult = d3d11Device->CreatePixelShader(g_ps_main, sizeof(g_ps_main), nullptr, &pixelShader);
        assert(SUCCEEDED(hResult));
    }
    ID3D11PixelShader* pixelShader2;
    {
        HRESULT hResult = d3d11Device->CreatePixelShader(g_ps2_main, sizeof(g_ps2_main), nullptr, &pixelShader2);
        assert(SUCCEEDED(hResult));
    }
    ID3D11PixelShader* pixelShader3;
    {
        HRESULT hResult = d3d11Device->CreatePixelShader(g_ps3_main, sizeof(g_ps3_main), nullptr, &pixelShader3);
        assert(SUCCEEDED(hResult));
    }

    // Create Input Layout
    ID3D11InputLayout* inputLayout;
    {
        D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
        {
            { "POS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };

        HRESULT hResult = d3d11Device->CreateInputLayout(inputElementDesc, ARRAYSIZE(inputElementDesc), g_vs_main, sizeof(g_vs_main), &inputLayout);
        assert(SUCCEEDED(hResult));
    }

    // Create Vertex Buffer
    ID3D11Buffer* vertexBuffer;
    UINT numVerts;
    UINT stride;
    UINT offset;
    {
        float vertexData[] = { // x, y, u, v
            -1.0f,  1.0f, 0.f, 0.f,
            1.0f, -1.0f, 1.f, 1.f,
            -1.0f, -1.0f, 0.f, 1.f,
            -1.0f,  1.0f, 0.f, 0.f,
            1.0f,  1.0f, 1.f, 0.f,
            1.0f, -1.0f, 1.f, 1.f
        };
        stride = 4 * sizeof(float);
        numVerts = sizeof(vertexData) / stride;
        offset = 0;

        D3D11_BUFFER_DESC vertexBufferDesc = {};
        vertexBufferDesc.ByteWidth = sizeof(vertexData);
        vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vertexSubresourceData = { vertexData };

        HRESULT hResult = d3d11Device->CreateBuffer(&vertexBufferDesc, &vertexSubresourceData, &vertexBuffer);
        assert(SUCCEEDED(hResult));
    }

    ID3D11Buffer* vertexBuffer2;
    UINT numVerts2;
    UINT stride2;
    UINT offset2;
    {
#define ALBW (67.0f / (PLAYER_WIDTH))
#define ALBH (67.0f / (PLAYER_HEIGHT))
#define ALBW2 (2.0f*ALBW)
#define ALBH2 (2.0f*ALBH)
#define ALBHT ((2.0f * ALBUM_HEIGHT) / PLAYER_HEIGHT)
#define ALBHM ((4.0f * ALBUM_HEIGHT) / PLAYER_HEIGHT)
#define ALBHB ((6.0f * ALBUM_HEIGHT) / PLAYER_HEIGHT)
        stride2 = 4 * sizeof(float);
        numVerts2 = 6*nAlbums;
        UINT vsize = numVerts2 * stride2;
        offset2 = 0;
        float* vd = (float*)malloc(vsize);
        float y = 0.0f;
        for (int i = 0; i < nAlbums; i++) {
            float cy = i == 0 ? ALBHT : i == nAlbums - 1 ? ALBHM : ALBHB;

            vd[i * 24 + 0] = -ALBW2;
            vd[i * 24 + 1] = y + ALBH2;
            vd[i * 24 + 2] = 0.5f - ALBW;
            vd[i * 24 + 3] = cy - ALBH;

            vd[i * 24 + 4] = ALBW2;
            vd[i * 24 + 5] = y - ALBH2;
            vd[i * 24 + 6] = 0.5f + ALBW;
            vd[i * 24 + 7] = cy + ALBH;

            vd[i * 24 + 8] = -ALBW2;
            vd[i * 24 + 9] = y - ALBH2;
            vd[i * 24 + 10] = 0.5f - ALBW;
            vd[i * 24 + 11] = cy + ALBH;

            vd[i * 24 + 12] = -ALBW2;
            vd[i * 24 + 13] = y + ALBH2;
            vd[i * 24 + 14] = 0.5f - ALBW;
            vd[i * 24 + 15] = cy - ALBH;

            vd[i * 24 + 16] = ALBW2;
            vd[i * 24 + 17] = y + ALBH2;
            vd[i * 24 + 18] = 0.5f + ALBW;
            vd[i * 24 + 19] = cy - ALBH;

            vd[i * 24 + 20] = ALBW2;
            vd[i * 24 + 21] = y - ALBH2;
            vd[i * 24 + 22] = 0.5f + ALBW;
            vd[i * 24 + 23] = cy + ALBH;

            y -= (ALBUM_HEIGHT * 4.0f) / PLAYER_HEIGHT;
        }
        /*
         { // x, y, u, v
            .0f - ALBW2, .0f + ALBH2, .5f-ALBW, ALBHF -ALBH,
            .0f + ALBW2, .0f - ALBH2, .5f+ALBW, ALBHF +ALBH,
            .0f - ALBW2, .0f - ALBH2, .5f-ALBW, ALBHF +ALBH,
            .0f - ALBW2, .0f + ALBH2, .5f-ALBW, ALBHF -ALBH,
            .0f + ALBW2, .0f + ALBH2, .5f+ALBW, ALBHF -ALBH,
            .0f + ALBW2, .0f - ALBH2, .5f+ALBW, ALBHF +ALBH,

            .0f - ALBW2, ALBH2*2 + ALBH2, .5f - ALBW, .5f - ALBH,
            .0f + ALBW2, ALBH2*2 - ALBH2, .5f + ALBW, .5f + ALBH,
            .0f - ALBW2, ALBH2*2 - ALBH2, .5f - ALBW, .5f + ALBH,
            .0f - ALBW2, ALBH2*2 + ALBH2, .5f - ALBW, .5f - ALBH,
            .0f + ALBW2, ALBH2*2 + ALBH2, .5f + ALBW, .5f - ALBH,
            .0f + ALBW2, ALBH2*2 - ALBH2, .5f + ALBW, .5f + ALBH,
        };*/
        /*float vertexData[] = { // x, y, u, v
            -1.0f,  1.0f, 0.f, 0.f,
            1.0f, -1.0f, 1.f, 1.f,
            -1.0f, -1.0f, 0.f, 1.f,
            -1.0f,  1.0f, 0.f, 0.f,
            1.0f,  1.0f, 1.f, 0.f,
            1.0f, -1.0f, 1.f, 1.f
        };*/
        

        D3D11_BUFFER_DESC vertexBufferDesc = {};
        vertexBufferDesc.ByteWidth = vsize;
        vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vertexSubresourceData = { vd };

        HRESULT hResult = d3d11Device->CreateBuffer(&vertexBufferDesc, &vertexSubresourceData, &vertexBuffer2);
        assert(SUCCEEDED(hResult));
    }

    // Create Sampler State
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.BorderColor[0] = 1.0f;
    samplerDesc.BorderColor[1] = 1.0f;
    samplerDesc.BorderColor[2] = 1.0f;
    samplerDesc.BorderColor[3] = 1.0f;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    ID3D11SamplerState* samplerState;
    d3d11Device->CreateSamplerState(&samplerDesc, &samplerState);

    // Create Texture
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = THUMBNAIL_SIZE;
    textureDesc.Height = THUMBNAIL_SIZE;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = nAlbums;
    textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    std::vector<D3D11_SUBRESOURCE_DATA> initialData(nAlbums);
    for (UINT i = 0; i < nAlbums; ++i)
    {
        initialData[i].pSysMem = albums[album_keys[i]].thumbnail;
        initialData[i].SysMemPitch = textureDesc.Width * 4; // 4 bytes per pixel for R8G8B8A8
    }
    // 3. Create the texture array resource with initial data
    ID3D11Texture2D* pTextureArray = nullptr;
    d3d11Device->CreateTexture2D(&textureDesc, initialData.data(), &pTextureArray);
    ID3D11ShaderResourceView* textureView;
    d3d11Device->CreateShaderResourceView(pTextureArray, nullptr, &textureView);

    RECT cr;
    GetClientRect(hwnd, &cr);

    D3D11_TEXTURE2D_DESC descStagingTexture;
    descStagingTexture = textureDesc;
    descStagingTexture.BindFlags = 0; // Staging textures cannot be bound to the pipeline
    descStagingTexture.Usage = D3D11_USAGE_DEFAULT;
    descStagingTexture.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    descStagingTexture.Width = cr.right - cr.left;
    descStagingTexture.Height = cr.bottom - cr.top;
    descStagingTexture.ArraySize = NBUTTONS;
    ID3D11Texture2D* basemaps = nullptr;
    d3d11Device->CreateTexture2D(&descStagingTexture, NULL, &basemaps);
    ID3D11ShaderResourceView* baseview;
    d3d11Device->CreateShaderResourceView(basemaps, nullptr, &baseview);
    ID3D11RenderTargetView* pRTVs[NBUTTONS];
    for (int i = 0; i < NBUTTONS; ++i) {
        D3D11_RENDER_TARGET_VIEW_DESC rtvdesc;
        rtvdesc.Format = descStagingTexture.Format;
        rtvdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvdesc.Texture2DArray.MipSlice = 0;
        rtvdesc.Texture2DArray.ArraySize = 1;
        rtvdesc.Texture2DArray.FirstArraySlice = i;
        d3d11Device->CreateRenderTargetView(basemaps, &rtvdesc, &pRTVs[i]);
    }

    // Create Constant Buffer
    struct Constants
    {
        int pressedButton;
    };
    struct Constants2
    {
        float accent[4];
        float progress;
        float panelx;
        int pressedButton;
        int playing;
        float a0, a1, a2, a3, a4, a5;
        float selpos[2];
    };
    struct Constants3
    {
        float pos[2];
        float selpos[2];
    };

    ID3D11Buffer* constantBuffer;
    {
        D3D11_BUFFER_DESC constantBufferDesc = {};
        // ByteWidth must be a multiple of 16, per the docs
        constantBufferDesc.ByteWidth = sizeof(Constants) + 0xf & 0xfffffff0;
        constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hResult = d3d11Device->CreateBuffer(&constantBufferDesc, nullptr, &constantBuffer);
        assert(SUCCEEDED(hResult));
    }
    ID3D11Buffer* constantBuffer2;
    {
        D3D11_BUFFER_DESC constantBufferDesc = {};
        // ByteWidth must be a multiple of 16, per the docs
        constantBufferDesc.ByteWidth = sizeof(Constants2) + 0xf & 0xfffffff0;
        constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hResult = d3d11Device->CreateBuffer(&constantBufferDesc, nullptr, &constantBuffer2);
        assert(SUCCEEDED(hResult));
    }
    ID3D11Buffer* constantBuffer3;
    {
        D3D11_BUFFER_DESC constantBufferDesc = {};
        // ByteWidth must be a multiple of 16, per the docs
        constantBufferDesc.ByteWidth = sizeof(Constants3) + 0xf & 0xfffffff0;
        constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hResult = d3d11Device->CreateBuffer(&constantBufferDesc, nullptr, &constantBuffer3);
        assert(SUCCEEDED(hResult));
    }

    ID2D1Factory* pD2DFactory = NULL;
    IDWriteFactory* pDWriteFactory = NULL;
    ID2D1RenderTarget* d2dRenderTarget = NULL;

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&pDWriteFactory));

    {
        // Get the backbuffer from the swap chain
        ID3D11Texture2D* d3d11FrameBuffer;
        HRESULT hResult = d3d11SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&d3d11FrameBuffer);
        assert(SUCCEEDED(hResult));

        // Query the backbuffer for its DXGI surface
        IDXGISurface* dxgiSurface = nullptr;
        d3d11FrameBuffer->QueryInterface(IID_PPV_ARGS(&dxgiSurface));

        // Create a Direct2D render target from the DXGI surface
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
        );
        pD2DFactory->CreateDxgiSurfaceRenderTarget(dxgiSurface, &props, &d2dRenderTarget);

        // Release the temporary interface pointers
        dxgiSurface->Release();
        d3d11FrameBuffer->Release();
    }

    d2dRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

    // Text format
    IDWriteTextFormat* textFormat = nullptr;
    pDWriteFactory->CreateTextFormat(
        L"Bahnschrift", // Font family name
        NULL,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        31.0f,
        L"en-us", // Locale
        &textFormat
    );
    textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

    IDWriteTextFormat* textFormat2 = nullptr;
    pDWriteFactory->CreateTextFormat(
        L"Bahnschrift", // Font family name
        NULL,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        14.0f,
        L"en-us", // Locale
        &textFormat2
    );
    textFormat2->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

    IDWriteTextFormat* textFormat3 = nullptr;
    pDWriteFactory->CreateTextFormat(
        L"Bahnschrift", // Font family name
        NULL,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        14.0f,
        L"en-us", // Locale
        &textFormat3
    );
    textFormat3->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    textFormat3->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    IDWriteInlineObject* pTrimmingSign = nullptr;
    pDWriteFactory->CreateEllipsisTrimmingSign(
        textFormat3, // Use the text format you intend to trim
        &pTrimmingSign
    );
    DWRITE_TRIMMING trimmingOptions = {
        DWRITE_TRIMMING_GRANULARITY_CHARACTER, // Granularity
        0, // Delimiter (not needed for simple ellipsis)
        0  // Delimiter Count
    };
    textFormat3->SetTrimming(
        &trimmingOptions,
        pTrimmingSign
    );

    D2D1_SIZE_F renderTargetSize = d2dRenderTarget->GetSize();

    // Text brush
    ID2D1SolidColorBrush* textBrush = nullptr;
    d2dRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(0.6549019607843137f, 0.6588235294117647f, 0.6666666666666666f),
        &textBrush
    );
    ID2D1SolidColorBrush* textBrush2 = nullptr;
    d2dRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(0.4588235294117647f, 0.4666666666666667f, 0.47843137254901963f),
        &textBrush2
    );

    QueryPerformanceFrequency(&freq);
    countsPerTick = (double)freq.QuadPart / TICKRATE;
    QueryPerformanceCounter(&t0);
    accum = 0.0;

    bool firstRun = true;

    for (int bi = 0; bi < NBUTTONS; bi++) {
        D3D11_MAPPED_SUBRESOURCE mappedSubresource;
        d3d11DeviceContext->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
        Constants* constants = (Constants*)(mappedSubresource.pData);
        constants->pressedButton = bi;
        d3d11DeviceContext->Unmap(constantBuffer, 0);

        d3d11DeviceContext->OMSetRenderTargets(1, &pRTVs[bi], nullptr);

        const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        d3d11DeviceContext->ClearRenderTargetView(pRTVs[bi], clearColor);

        D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (FLOAT)(descStagingTexture.Width), (FLOAT)(descStagingTexture.Height), 0.0f, 1.0f };
        d3d11DeviceContext->RSSetViewports(1, &viewport);

        d3d11DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3d11DeviceContext->IASetInputLayout(inputLayout);

        d3d11DeviceContext->VSSetShader(vertexShader, nullptr, 0);
        d3d11DeviceContext->PSSetShader(pixelShader, nullptr, 0);

        d3d11DeviceContext->PSSetConstantBuffers(0, 1, &constantBuffer);

        d3d11DeviceContext->PSSetShaderResources(0, 1, &textureView);
        d3d11DeviceContext->PSSetSamplers(0, 1, &samplerState);

        d3d11DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

        d3d11DeviceContext->Draw(numVerts, 0);
    }

    // Main Loop
    bool isRunning = true;
    while (isRunning)
    {
        MSG msg = {};
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                isRunning = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        POINT cursor;
        GetCursorPos(&cursor);
        ScreenToClient(hwnd, &cursor);
        selAlbum = ((cursor.y - curpanely + ALBUM_HEIGHT) / (2 * ALBUM_HEIGHT));
        if (cursor.x > 268 || cursor.x < 0 || selAlbum < 0 || selAlbum >= nAlbums) {
            selActive = false;
        }
        else if (panelx == 1.0f){
            selActive = true;
        }

        feedAudio();

        QueryPerformanceCounter(&t1);
        LARGE_INTEGER dif;
        dif.QuadPart = t1.QuadPart - t0.QuadPart;
        t0 = t1;
        accum += (double)dif.QuadPart;
        frameDeltaSec = ((double)dif.QuadPart / freq.QuadPart);
        tickloop();

        {
            D3D11_MAPPED_SUBRESOURCE mappedSubresource;
            d3d11DeviceContext->Map(constantBuffer2, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
            Constants2* constants = (Constants2*)(mappedSubresource.pData);
            constants->accent[3] = useAccent && isFocused;
            constants->accent[2] = ((accentColor >> 16) & 0xff) / 255.0f;
            constants->accent[1] = ((accentColor >> 8) & 0xff) / 255.0f;
            constants->accent[0] = (accentColor & 0xff) / 255.0f;
            constants->progress = progress;
            constants->panelx = panelx;
            constants->pressedButton = ldownid == hoveredid ? ldownid + 1 : 0;
            constants->playing = playing;
            constants->a0 = amplitudes[0];
            constants->a1 = amplitudes[1];
            constants->a2 = amplitudes[2];
            constants->a3 = amplitudes[3];
            constants->a4 = amplitudes[4];
            constants->a5 = amplitudes[5];
            constants->selpos[0] = selx;
            constants->selpos[1] = sely;
            d3d11DeviceContext->Unmap(constantBuffer2, 0);
        }

        FLOAT backgroundColor[4] = { 0.1f, 0.2f, 0.6f, 1.0f };
        d3d11DeviceContext->ClearRenderTargetView(d3d11FrameBufferView, backgroundColor);

        RECT winRect;
        GetClientRect(hwnd, &winRect);
        D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (FLOAT)(winRect.right - winRect.left), (FLOAT)(winRect.bottom - winRect.top), 0.0f, 1.0f };
        d3d11DeviceContext->RSSetViewports(1, &viewport);

        d3d11DeviceContext->OMSetRenderTargets(1, &d3d11FrameBufferView, nullptr);

        d3d11DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3d11DeviceContext->IASetInputLayout(inputLayout);

        d3d11DeviceContext->VSSetShader(vertexShader, nullptr, 0);
        d3d11DeviceContext->PSSetShader(pixelShader2, nullptr, 0);

        d3d11DeviceContext->PSSetConstantBuffers(0, 1, &constantBuffer2);

        d3d11DeviceContext->PSSetShaderResources(0, 1, &textureView);
        d3d11DeviceContext->PSSetShaderResources(1, 1, &baseview);
        d3d11DeviceContext->PSSetSamplers(0, 1, &samplerState);

        d3d11DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

        d3d11DeviceContext->Draw(numVerts, 0);

        {
            D3D11_MAPPED_SUBRESOURCE mappedSubresource;
            d3d11DeviceContext->Map(constantBuffer3, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
            Constants3* constants = (Constants3*)(mappedSubresource.pData);
            constants->pos[0] = (panelx - 0.85f) * 2.0f - 1.0f;
            constants->pos[1] = (-panely/PLAYER_HEIGHT) * 2.0f + 1.0f;
            constants->selpos[0] = selx;
            constants->selpos[1] = sely;
            d3d11DeviceContext->Unmap(constantBuffer3, 0);

            d3d11DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            d3d11DeviceContext->IASetInputLayout(inputLayout);

            d3d11DeviceContext->VSSetShader(vertexShader2, nullptr, 0);
            d3d11DeviceContext->PSSetShader(pixelShader3, nullptr, 0);

            d3d11DeviceContext->VSSetConstantBuffers(0, 1, &constantBuffer3);
            d3d11DeviceContext->PSSetConstantBuffers(0, 1, &constantBuffer3);

            d3d11DeviceContext->PSSetShaderResources(0, 1, &textureView);
            d3d11DeviceContext->PSSetShaderResources(1, 1, &baseview);
            d3d11DeviceContext->PSSetSamplers(0, 1, &samplerState);

            d3d11DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer2, &stride2, &offset2);

            d3d11DeviceContext->Draw(numVerts2, 0);
        }

        // Perform D2D rendering
        d2dRenderTarget->BeginDraw();

        D2D1_RECT_F clipRect = D2D1::RectF(panelx*PLAYER_WIDTH, 0.0f, PLAYER_WIDTH, PLAYER_HEIGHT);
        
        d2dRenderTarget->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_ALIASED);

        // Draw the text
        {
            const wchar_t* text = L"Poisoned";
            d2dRenderTarget->DrawText(
                text,
                wcslen(text),
                textFormat,
                D2D1::RectF(0, 288 * SCALE, PLAYER_WIDTH, 308 * SCALE),
                textBrush
            );
        }

        {
            const wchar_t* text = L"The Third Twin";
            d2dRenderTarget->DrawText(
                text,
                wcslen(text),
                textFormat2,
                D2D1::RectF(0, 315 * SCALE, PLAYER_WIDTH, 324 * SCALE),
                textBrush2
            );
        }

        d2dRenderTarget->PopAxisAlignedClip();

        float albumY = panely;
        for (int i = 0; i < nAlbums; i++)
        {
            d2dRenderTarget->DrawText(
                album_keys[i].c_str(),
                wcslen(album_keys[i].c_str()),
                textFormat3,
                D2D1::RectF((panelx - 1.0f) * PLAYER_WIDTH + (40 + 35) * SCALE, albumY - 13 * SCALE, PLAYER_WIDTH*2, PLAYER_HEIGHT*2),
                textBrush
            );

            d2dRenderTarget->DrawText(
                albums[album_keys[i]].artist.c_str(),
                wcslen(albums[album_keys[i]].artist.c_str()),
                textFormat3,
                D2D1::RectF((panelx - 1.0f) * PLAYER_WIDTH + (40 + 35) * SCALE, albumY + 2 * SCALE, PLAYER_WIDTH * 2, PLAYER_HEIGHT * 2),
                textBrush2
            );

            /*for (int j = 0; j < albums[album_keys[i]].songs.size(); j++) {
                d2dRenderTarget->DrawText(
                    albums[album_keys[i]].songs[j].title.c_str(),
                    wcslen(albums[album_keys[i]].songs[j].title.c_str()),
                    textFormat3,
                    D2D1::RectF((panelx - 1.0f) * PLAYER_WIDTH + 30 * SCALE, albumY + ALBUM_HEIGHT + j * SONG_HEIGHT + 13, panelx * PLAYER_WIDTH - 80 * SCALE, PLAYER_HEIGHT * 2),
                    textBrush
                );
            }*/
            albumY += ALBUM_HEIGHT * 2;// +SONG_HEIGHT * (albums[album_keys[i]].songs.size() + 1);
        }

        d2dRenderTarget->EndDraw();

        d3d11SwapChain->Present(1, 0);
    }


    return 0;
}