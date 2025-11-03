#include "DarkPlayer.h"

IMFSourceReader* pReader = nullptr;
IMFMediaType* pAudioType = nullptr;
IMFSample* pSample = nullptr;
IMFMediaBuffer* pBuffer = nullptr;

IXAudio2* pXAudio2 = nullptr;
IXAudio2MasteringVoice* pMasteringVoice = nullptr;
IXAudio2SourceVoice* pSourceVoice = nullptr;

float amplitudes[VISBARS];

template <class T> void SafeRelease(T** ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = nullptr;
    }
}

void init_audio() {
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        std::cerr << "MFStartup failed" << std::endl;
        CoUninitialize();
        ExitProcess(hr);
    }

    hr = XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);

    hr = pXAudio2->CreateMasteringVoice(&pMasteringVoice);

    WAVEFORMATEX wfx = { 0 };
    wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = 44100;
    wfx.wBitsPerSample = 32;
    wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8);
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    hr = pXAudio2->CreateSourceVoice(&pSourceVoice, &wfx);
}

void play() {
    HRESULT hr = pSourceVoice->Start(0);
    if (SUCCEEDED(hr)) playing = true;
}

void pause() {
    HRESULT hr = pSourceVoice->Stop(0);
    if (SUCCEEDED(hr)) playing = false;
}

int BinSrch(int freq)
{
    int i;
    if (freq <= 20 || freq > 20000)
        return -1;
    freq -= 20;
    for (i = 0; i < 60; i++)
    {
        if (freq > (i * 333) && freq <= (i + 1) * 333)
            break;
    }
    return i;
}

void feedAudio() {
#define BUFSZ 44100
    static int curbuf = 0;
    static float buf[BUFSZ * 2 * 3];
    static std::vector<float> samples;
    static int lastSP = 0;
    static int playhead = 0;
    while (1) {
        XAUDIO2_VOICE_STATE state;
        pSourceVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (state.BuffersQueued >= 2) break;
        float* cur = buf + curbuf * BUFSZ * 2;
        while (samples.size() < BUFSZ * 2) {
            DWORD streamFlags = 0;
            HRESULT hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &streamFlags, NULL, &pSample);
            if (FAILED(hr)) break;

            if (streamFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
                std::cout << "End of stream reached." << std::endl;
                break;
            }

            if (pSample) {
                hr = pSample->ConvertToContiguousBuffer(&pBuffer);
                if (FAILED(hr)) break;

                BYTE* pAudioBytes = nullptr;
                DWORD cbAudioBytes = 0;
                hr = pBuffer->Lock(&pAudioBytes, NULL, &cbAudioBytes);
                if (FAILED(hr)) break;

                samples.insert(samples.end(), (float*)pAudioBytes, (float*)(pAudioBytes + cbAudioBytes));

                pBuffer->Unlock();
                SafeRelease(&pBuffer);
                SafeRelease(&pSample);
            }
        }
        UINT len = std::min((int)samples.size(), BUFSZ * 2);
        std::memcpy(cur, samples.data(), len * sizeof(float));
        samples.erase(samples.begin(), samples.begin() + len);

        XAUDIO2_BUFFER xabuf = { 0 };
        xabuf.AudioBytes = (UINT32)(len * sizeof(float));
        xabuf.pAudioData = (BYTE*)(cur);
        if (len < BUFSZ * 2) {
            //xabuf.Flags = XAUDIO2_END_OF_STREAM;
        }

        HRESULT hr = pSourceVoice->SubmitSourceBuffer(&xabuf);
        if (FAILED(hr)) break;

        curbuf = (curbuf + 1) % 3;
    }

    const int N_SAMPLES = 1024;
    const int N_BINS = N_SAMPLES / 2 + 1;

    static float in[N_SAMPLES];

    static kiss_fft_cpx out[N_BINS];

    XAUDIO2_VOICE_STATE state;
    pSourceVoice->GetState(&state);
    playhead = (playhead + state.SamplesPlayed - lastSP) % (3 * BUFSZ);
    lastSP = state.SamplesPlayed;

    elapsedSec = (state.SamplesPlayed / 44100.0);
    progress = elapsedSec / currentSongDuration;

    static bool hannInitialized = false;
    static float hann[N_SAMPLES];
    if (!hannInitialized) {
        float constant_multiplier = 2.0 * M_PI / (N_SAMPLES - 1);
        for (int n = 0; n < N_SAMPLES; ++n) {
            hann[n] = 0.5f * (1.0f - cosf(constant_multiplier * n));;
        }
        hannInitialized = true;
    }

    int start = playhead - N_SAMPLES / 2;
    if (start < 0) start = 3 * BUFSZ + start;
    for (int i = 0; i < N_SAMPLES; i++) {
        in[i] = (buf[((start + i) % (3 * BUFSZ)) * 2] + buf[((start + i) % (3 * BUFSZ)) * 2 + 1]) * 0.5f * hann[i];
    }

    kiss_fftr_cfg cfg = kiss_fftr_alloc(N_SAMPLES, 0, nullptr, nullptr);

    kiss_fftr(cfg, in, out);

    kiss_fftr_free(cfg);

    static float mags[N_BINS];

    // "Squash" into the Logarithmic Scale
    static float step = 1.06;
    float lowf = 1.0f;
    size_t m = 0;
    float max_amp = 1.0f;
    for (float f = lowf; (size_t)f < N_SAMPLES / 2; f = ceilf(f * step)) {
        float f1 = ceilf(f * step);
        float a = 0.0f;
        for (size_t q = (size_t)f; q < N_SAMPLES / 2 && q < (size_t)f1; ++q) {
            float b = sqrtf((out[q].r * out[q].r) + (out[q].i * out[q].i));
            if (b > a) a = b;
        }
        if (max_amp < a) max_amp = a;
        mags[m++] = a;
    }

    // Normalize Frequencies to 0..1 range
    for (size_t i = 0; i < m; ++i) {
        mags[i] /= max_amp;
    }

    //printf("step: %f m: %d\n", step, m);
    if (m > 10) step += 0.001;

    static float smooth[6];

    // Smooth out and smear the values
    for (size_t i = 0; i < 6; ++i) {
        float smoothness = 32;
        smooth[i] += (mags[i] - smooth[i]) * std::min(1.0f, smoothness * frameDeltaSec);
        float smearness = 24;
        amplitudes[i] += (smooth[i] - amplitudes[i]) * std::min(1.0f, smearness * frameDeltaSec);
    }

}

double getMediaDurationSec(const WCHAR* filePath){
    IMFMediaSource* pMSource = nullptr;
    IMFPresentationDescriptor* pPD = nullptr;
    PROPVARIANT var;
    HRESULT hr = S_OK;

    MF_OBJECT_TYPE ObjectType = MF_OBJECT_INVALID;

    IMFSourceResolver* pSourceResolver = NULL;
    IUnknown* pSource = NULL;

    UINT64 duration100ns = 0;

    hr = MFCreateSourceResolver(&pSourceResolver);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pSourceResolver->CreateObjectFromURL(
        filePath,                       // URL of the source.
        MF_RESOLUTION_MEDIASOURCE,  // Create a source object.
        NULL,                       // Optional property store.
        &ObjectType,        // Receives the created object type. 
        &pSource            // Receives a pointer to the media source.
    );
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pSource->QueryInterface(IID_PPV_ARGS(&pMSource));

    if (SUCCEEDED(hr))
    {
        hr = pMSource->CreatePresentationDescriptor(&pPD);
    }

    if (SUCCEEDED(hr))
    {
        hr = pPD->GetUINT64(MF_PD_DURATION, &duration100ns);
    }

done:
    PropVariantClear(&var);
    SafeRelease(&pPD);
    SafeRelease(&pMSource);
    SafeRelease(&pSource);
    SafeRelease(&pSourceResolver);

    return (double)duration100ns * 100.0 / 1e9;
}

HRESULT loadSong(std::wstring input_file) {

    HRESULT hr = MFCreateSourceReaderFromURL(input_file.c_str(), NULL, &pReader);
    if (FAILED(hr)) {
        std::cerr << "Failed to create source reader for input file: " << std::endl;
        return hr;
    }

    IMFMediaType* pOutputMediaType = nullptr;

    hr = MFCreateMediaType(&pOutputMediaType);

    hr = pOutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);

    hr = pOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);

    hr = pOutputMediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);

    hr = pOutputMediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);

    hr = pOutputMediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32);

    hr = pOutputMediaType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2 * 4);

    hr = pOutputMediaType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 44100 * 2 * 4);

    hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, pOutputMediaType);
    if (FAILED(hr)) {
        std::cerr << "Failed to set complete output format." << std::endl;
        return hr;
    }

    SafeRelease(&pOutputMediaType);

    currentSongDuration = getMediaDurationSec(input_file.c_str());

    return hr;
}