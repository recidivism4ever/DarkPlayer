#pragma once

#include "resource.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>

#include <d3d11_1.h>
#pragma comment(lib, "d3d11.lib")

#include <assert.h>

#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

void init_image_loader();

BYTE* load_image(bool flip_vertically, UINT* width, UINT* height, LPCWSTR path);

#include "playerdefs.h"

