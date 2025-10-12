#include "DarkPlayer.h"

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
