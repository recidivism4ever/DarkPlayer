#include <windows.h>
#include <shlobj.h>
#include <propvarutil.h>
#include <propsys.h>
#include <Propkey.h>
#include <iostream>
#include <string>
#include <map>
#include <vector>

#pragma comment(lib, "Propsys.lib")

// Function to get the album art thumbnail
bool getAlbumArtThumbnail(IShellItem* psi) {
    IShellItemImageFactory* pFactory = nullptr;
    HRESULT hr = psi->BindToHandler(NULL, BHID_SFUIObject, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr)) {
        return false;
    }

    HBITMAP hBitmap;
    // Request a 64x64 thumbnail
    hr = pFactory->GetImage({ 64, 64 }, SIIGBF_THUMBNAILONLY, &hBitmap);
    pFactory->Release();

    if (SUCCEEDED(hr) && hBitmap) {
        //image.Attach(hBitmap);
        return true;
    }
    return false;
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

// Recursive function to find music items and their albums
void findMusicInFolder(IShellItem* psiFolder,
    std::map<std::wstring, std::vector<std::wstring>>& albums) {
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
                    albums[albumTitle].push_back(pwszPath);
                    CoTaskMemFree(pwszPath);
                }
            }
        }
        psi->Release();
    }
    pesi->Release();
}

void iterateAlbums() {

    IShellItem* psiMusicFolder = nullptr;
    HRESULT hr = SHGetKnownFolderItem(FOLDERID_Music, KF_FLAG_DEFAULT, NULL, IID_PPV_ARGS(&psiMusicFolder));

    if (SUCCEEDED(hr)) {
        std::map<std::wstring, std::vector<std::wstring>> albums;
        findMusicInFolder(psiMusicFolder, albums);

        for (const auto& pair : albums) {
            std::wcout << L"Album: " << pair.first << std::endl;
            for (const auto& track : pair.second) {
                std::wcout << L"\t- " << track << std::endl;
            }
            std::wcout << std::endl;
        }
        psiMusicFolder->Release();
    }
    else {
        std::wcerr << L"Failed to get music folder." << std::endl;
    }

}
