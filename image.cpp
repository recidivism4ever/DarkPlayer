#include "DarkPlayer.h"

static IWICImagingFactory2* ifactory;

void init_image_loader() {
    CoCreateInstance(CLSID_WICImagingFactory2, 0, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory2, (LPVOID*)&ifactory);
}

BYTE* load_image(bool flip_vertically, UINT* width, UINT* height, LPCWSTR path) {
    IWICBitmapDecoder* pDecoder = NULL;
    IWICBitmapFrameDecode* pFrame = NULL;
    IWICBitmapSource* convertedSrc = NULL;

    HRESULT hr = ifactory->CreateDecoderFromFilename(path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &pDecoder);
    pDecoder->GetFrame(0, &pFrame);
    WICConvertBitmapSource(GUID_WICPixelFormat32bppRGBA, (IWICBitmapSource*)pFrame, &convertedSrc);
    convertedSrc->GetSize(width, height);
    UINT size = width[0] * height[0] * sizeof(UINT);
    UINT rowPitch = width[0] * sizeof(UINT);
    BYTE* pixels = (BYTE*)malloc(size);
    if (flip_vertically) {
        IWICBitmapFlipRotator* pFlipRotator;
        ifactory->CreateBitmapFlipRotator(&pFlipRotator);
        pFlipRotator->Initialize(convertedSrc, WICBitmapTransformFlipVertical);
        pFlipRotator->CopyPixels(NULL, rowPitch, size, pixels);
        pFlipRotator->Release();
    }
    else {
        convertedSrc->CopyPixels(NULL, rowPitch, size, pixels);
    }
    convertedSrc->Release();
    pFrame->Release();
    pDecoder->Release();

    return pixels;
}