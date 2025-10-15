#pragma once

#include "resource.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#define _USE_MATH_DEFINES
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

#include <xaudio2.h>
#include <xaudio2fx.h>
#pragma comment(lib, "xaudio2.lib")

#include <mfidl.h>
#include <mfobjects.h>
#include <mferror.h>
#include <mfapi.h>
#include <mfreadwrite.h>
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")

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


#include "kiss_fftr.h"

void init_image_loader();

BYTE* load_image(bool flip_vertically, UINT* width, UINT* height, LPCWSTR path);

#include "playerdefs.h"
extern float amplitudes[VISBARS];

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
    bool thumbnailFound;
    BYTE thumbnail[THUMBNAIL_SIZE * THUMBNAIL_SIZE * 4];
};

std::map<std::wstring, Album> iterateAlbums();

void init_audio();

HRESULT loadSong(std::wstring input_file);

void play();

void pause();

void feedAudio();