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
#include <array>

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
    double durationSec;

    bool operator<(const Song& other) const {
        return number < other.number;
    }
};

struct ActiveSong {
    std::wstring title;
    std::wstring artist;
    double durationSec;
    bool thumbnailFound;
    BYTE thumbnail[THUMBNAIL_SIZE * THUMBNAIL_SIZE * 4];
};

extern int activeSong;
extern ActiveSong activeSong2;

extern ID3D11DeviceContext1* d3d11DeviceContext;
extern ID3D11Texture2D* albumTexture;

struct Album {
    std::vector<Song> songs;
    std::wstring artist;
    bool thumbnailFound;
    BYTE thumbnail[THUMBNAIL_SIZE * THUMBNAIL_SIZE * 4];
};

std::map<std::wstring, Album> iterateAlbums();

void init_audio();

HRESULT loadSong(std::wstring input_file);

extern bool playing;

void play();

void pause();

void seekTo(float frac);

void feedAudio();

extern float frameDeltaSec;

extern float progress;

extern double elapsedSec;

double getMediaDurationSec(const WCHAR* filePath);

std::wstring getSongTitle(IShellItem* psi);

std::wstring getArtist(IShellItem* psi);

bool getThumbnail(IShellItem* psi, BYTE* buffer);

double getDurationSec(IShellItem* psi);