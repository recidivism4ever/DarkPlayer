#pragma once

#include "resource.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include <math.h>
#include <stdio.h>

#include <d3d11_1.h>
#pragma comment(lib, "d3d11.lib")

#include <d2d1.h>
#pragma comment(lib, "d2d1.lib")

#include <dwrite.h>
#pragma comment(lib, "dwrite.lib")

#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

#include <d3dcsx.h>

#include <shlobj.h>
#include <propvarutil.h>
#include <Propkey.h>
#include <propsys.h>
#pragma comment(lib, "Propsys.lib")

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <algorithm>

#include <assert.h>


void init_image_loader();

BYTE* load_image(bool flip_vertically, UINT* width, UINT* height, LPCWSTR path);

#include "playerdefs.h"

struct Song {
    std::wstring path;
    std::wstring title;
    UINT number;

    bool operator<(const Song& other) const {
        return number < other.number;
    }
};

struct Album {
    std::vector<Song> songs;
    std::wstring artist;
    HBITMAP bitmap;
};

std::map<std::wstring, Album> iterateAlbums();