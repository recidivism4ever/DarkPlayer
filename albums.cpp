#include "DarkPlayer.h"
#include <wrl/client.h>
#include <thumbcache.h>

using namespace Microsoft::WRL;

HRESULT GetScaledPixelsFromStream(
    PROPVARIANT& pv,
    BYTE *buffer
){
    ComPtr<IStream> pStream;
    HRESULT hr = pv.punkVal->QueryInterface(IID_PPV_ARGS(&pStream));
    if (FAILED(hr)) return hr;

    ComPtr<IWICImagingFactory> pFactory;
    hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr)) return hr;

    ComPtr<IWICBitmapDecoder> pDecoder;
    hr = pFactory->CreateDecoderFromStream(pStream.Get(), NULL, WICDecodeMetadataCacheOnLoad, &pDecoder);
    if (FAILED(hr)) return hr;

    ComPtr<IWICBitmapFrameDecode> pFrame;
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr)) return hr;

    // Convert to a common pixel format (e.g., 32bpp RGBA) for D3D
    ComPtr<IWICFormatConverter> pConverter;
    hr = pFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr)) return hr;

    hr = pConverter->Initialize(
        pFrame.Get(),
        GUID_WICPixelFormat32bppPBGRA, // Pixel format compatible with D3D11
        WICBitmapDitherTypeNone,
        NULL,
        0.f,
        WICBitmapPaletteTypeCustom
    );
    if (FAILED(hr)) return hr;

    // Create the scaler
    ComPtr<IWICBitmapScaler> pScaler;
    hr = pFactory->CreateBitmapScaler(&pScaler);
    if (FAILED(hr)) return hr;

    hr = pScaler->Initialize(
        pConverter.Get(),
        THUMBNAIL_SIZE,
        THUMBNAIL_SIZE,
        WICBitmapInterpolationModeFant // High-quality scaling
    );
    if (FAILED(hr)) return hr;

    // Copy the scaled pixels into the buffer
    hr = pScaler->CopyPixels(NULL, THUMBNAIL_SIZE*4, THUMBNAIL_SIZE * THUMBNAIL_SIZE * 4, buffer);

    return hr;
}

HRESULT getThumbnail(IShellItem* psi, Album *a) {
    if (a->thumbnail == NULL) return NULL;

    IPropertyStore* pps = nullptr;
    HRESULT hr = psi->BindToHandler(NULL, BHID_PropertyStore, IID_PPV_ARGS(&pps));
    if (FAILED(hr)) {
        return NULL; // Return empty on failure
    }

    PROPVARIANT pv;
    PropVariantInit(&pv);

    hr = pps->GetValue(PKEY_ThumbnailStream, &pv);
    if (SUCCEEDED(hr)) {
        hr = GetScaledPixelsFromStream(pv, a->thumbnail);
        if (SUCCEEDED(hr)) a->thumbnailFound = true;
        PropVariantClear(&pv);
    }

    pps->Release();

    /*

    ComPtr<IThumbnailCache> pThumbnailCache;
    HRESULT hr = CoCreateInstance(CLSID_LocalThumbnailCache, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pThumbnailCache));
    if (FAILED(hr)) {
        return hr;
    }

    ComPtr<ISharedBitmap> pSharedBitmap;
    hr = pThumbnailCache->GetThumbnail(
        psi,
        256, // Desired thumbnail size (256x256 pixels)
        WTS_FORCEEXTRACTION | WTS_SCALEUP | WTS_CROPTOSQUARE, // WTS_EXTRACT to force extraction if not in cache, WTS_FORCEEXTRACTION to always extract.
        &pSharedBitmap,
        nullptr,
        nullptr);
    if (SUCCEEDED(hr)) {
        HBITMAP hbm;
        hr = pSharedBitmap->GetSharedBitmap(&hbm);
        BITMAP bm;
        int r = GetObject(hbm, sizeof(bm), &bm);
        if (r) {
            BITMAPINFOHEADER bih;
            bih.biSize = sizeof(BITMAPINFOHEADER);
            bih.biWidth = THUMBNAIL_SIZE;
            bih.biHeight = -THUMBNAIL_SIZE;
            bih.biPlanes = 1;
            bih.biBitCount = 32; // Use 32-bit for consistent RGBX format
            bih.biCompression = BI_RGB; // Important: Force BI_RGB to avoid issues

            HDC hdc = GetDC(NULL);
            GetDIBits(hdc, hbm, 0, THUMBNAIL_SIZE, a->thumbnail, (BITMAPINFO*)&bih, DIB_RGB_COLORS);
            ReleaseDC(NULL, hdc);

            a->thumbnailFound = true;
        }
    }
    */

    return hr;
}

// A helper to get the album title from an IShellItem
std::wstring getAlbumTitle(IShellItem* psi) {
    IPropertyStore* pps = nullptr;
    HRESULT hr = psi->BindToHandler(NULL, BHID_PropertyStore, IID_PPV_ARGS(&pps));
    if (FAILED(hr)) {
        return L""; // Return empty on failure
    }

    PROPVARIANT pv;
    PropVariantInit(&pv);

    hr = pps->GetValue(PKEY_Music_AlbumTitle, &pv);
    std::wstring albumTitle;
    if (SUCCEEDED(hr)) {
        PWSTR pwszAlbumTitle = nullptr;
        PropVariantToStringAlloc(pv, &pwszAlbumTitle);
        if (pwszAlbumTitle) {
            albumTitle = pwszAlbumTitle;
            CoTaskMemFree(pwszAlbumTitle);
        }
        PropVariantClear(&pv);
    }
    pps->Release();
    return albumTitle;
}

std::wstring getArtist(IShellItem* psi) {
    IPropertyStore* pps = nullptr;
    HRESULT hr = psi->BindToHandler(NULL, BHID_PropertyStore, IID_PPV_ARGS(&pps));
    if (FAILED(hr)) {
        return L""; // Return empty on failure
    }

    PROPVARIANT pv;
    PropVariantInit(&pv);

    hr = pps->GetValue(PKEY_Music_Artist, &pv);
    std::wstring artist;
    if (SUCCEEDED(hr)) {
        PWSTR pwszArtist = nullptr;
        PropVariantToStringAlloc(pv, &pwszArtist);
        if (pwszArtist) {
            artist = pwszArtist;
            CoTaskMemFree(pwszArtist);
        }
        PropVariantClear(&pv);
    }
    pps->Release();
    return artist;
}

std::wstring getSongTitle(IShellItem* psi) {
    IPropertyStore* pps = nullptr;
    HRESULT hr = psi->BindToHandler(NULL, BHID_PropertyStore, IID_PPV_ARGS(&pps));
    if (FAILED(hr)) {
        return L"";
    }

    PROPVARIANT pv;
    PropVariantInit(&pv);

    hr = pps->GetValue(PKEY_Title, &pv);
    std::wstring songTitle;
    if (SUCCEEDED(hr)) {
        PWSTR pwszSongTitle = nullptr;
        PropVariantToStringAlloc(pv, &pwszSongTitle);
        if (pwszSongTitle) {
            songTitle = pwszSongTitle;
            CoTaskMemFree(pwszSongTitle);
        }
        PropVariantClear(&pv);
    }
    pps->Release();
    return songTitle;
}

UINT32 getTrackNumber(IShellItem* psi) {
    IPropertyStore* pps = nullptr;
    HRESULT hr = psi->BindToHandler(NULL, BHID_PropertyStore, IID_PPV_ARGS(&pps));
    if (FAILED(hr)) {
        return 0;
    }

    PROPVARIANT pv;
    PropVariantInit(&pv);

    hr = pps->GetValue(PKEY_Music_TrackNumber, &pv);
    UINT32 trackNumber = 0;
    if (SUCCEEDED(hr)) {
        // Use PropVariantToUInt32 to safely convert the PROPVARIANT
        PropVariantToUInt32(pv, &trackNumber);
        PropVariantClear(&pv);
    }
    pps->Release();
    return trackNumber;
}

// Recursive function to find music items and their albums
void findMusicInFolder(IShellItem* psiFolder,
    std::map<std::wstring, Album>& albums) {
    IEnumShellItems* pesi = nullptr;
    // Get an enumerator for the items in this folder
    HRESULT hr = psiFolder->BindToHandler(NULL, BHID_EnumItems, IID_PPV_ARGS(&pesi));
    if (FAILED(hr)) {
        return;
    }

    IShellItem* psi = nullptr;
    while (pesi->Next(1, &psi, NULL) == S_OK) {
        SFGAOF attributes;
        psi->GetAttributes(SFGAO_FOLDER, &attributes);

        if (attributes & SFGAO_FOLDER) {
            // It's a subfolder, recurse into it
            findMusicInFolder(psi, albums);
        }
        else {
            // It's a file, get its album title and path
            std::wstring albumTitle = getAlbumTitle(psi);
            if (!albumTitle.empty()) {
                PWSTR pwszPath = nullptr;
                psi->GetDisplayName(SIGDN_FILESYSPATH, &pwszPath);
                if (pwszPath) {
                    Song s = {
                        pwszPath, getSongTitle(psi), getTrackNumber(psi)
                    };
                    if (albums[albumTitle].artist.empty()) {
                        albums[albumTitle].artist = getArtist(psi);
                    }
                    if (!albums[albumTitle].thumbnailFound) {
                        getThumbnail(psi, &(albums[albumTitle]));
                    }
                    albums[albumTitle].songs.push_back(s);
                    CoTaskMemFree(pwszPath);
                }
            }
        }
        psi->Release();
    }
    pesi->Release();
}

std::map<std::wstring, Album> iterateAlbums(){

    std::map<std::wstring, Album> albums;

    IShellItem* psiMusicFolder = nullptr;
    HRESULT hr = SHGetKnownFolderItem(FOLDERID_Music, KF_FLAG_DEFAULT, NULL, IID_PPV_ARGS(&psiMusicFolder));

    if (SUCCEEDED(hr)) {
        findMusicInFolder(psiMusicFolder, albums);

        for (auto& pair : albums) {
            std::wcout << L"Album: " << pair.first << " - " << pair.second.artist << std::endl;
            std::sort(pair.second.songs.begin(), pair.second.songs.end());
            for (auto& track : pair.second.songs) {
                std::wcout << L"\t- " << track.number << ": " << track.title << std::endl;
            }
            std::wcout << std::endl;
        }
        psiMusicFolder->Release();
    }
    else {
        std::wcerr << L"Failed to get music folder." << std::endl;
    }

    return albums;
}
