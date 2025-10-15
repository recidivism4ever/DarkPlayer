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

    auto hmod = LoadLibraryW(D3DCSX_DLL_W);
    if (!hmod) {
        CoUninitialize();
        ExitProcess(hr);
    }

    hr = XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);

    // Create a mastering voice for the system's default audio device
    hr = pXAudio2->CreateMasteringVoice(&pMasteringVoice);

    // Define the format of your audio data
    WAVEFORMATEX wfx = { 0 };
    wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT; // Our PCM data is in floating-point format
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = 44100;
    wfx.wBitsPerSample = 32; // sizeof(float)
    wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8);
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    // Create a source voice to play the sound
    hr = pXAudio2->CreateSourceVoice(&pSourceVoice, &wfx);
}

// Assumes pcm_data is a vector of floats, sampleRate, and channels are known
void PlayAudioWithXAudio2(const std::vector<float>& pcm_data, UINT32 sampleRate, UINT32 channels) {

    HRESULT hr = XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);

    // Create a mastering voice for the system's default audio device
    hr = pXAudio2->CreateMasteringVoice(&pMasteringVoice);

    // Define the format of your audio data
    WAVEFORMATEX wfx = { 0 };
    wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT; // Our PCM data is in floating-point format
    wfx.nChannels = channels;
    wfx.nSamplesPerSec = sampleRate;
    wfx.wBitsPerSample = 32; // sizeof(float)
    wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8);
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    // Create a source voice to play the sound
    hr = pXAudio2->CreateSourceVoice(&pSourceVoice, &wfx);

    // Create an XAUDIO2_BUFFER structure to describe our audio data
    XAUDIO2_BUFFER buffer = { 0 };
    buffer.AudioBytes = static_cast<UINT32>(pcm_data.size() * sizeof(float));
    buffer.pAudioData = reinterpret_cast<BYTE*>(const_cast<float*>(pcm_data.data()));

    // Submit the buffer and start playback
    hr = pSourceVoice->SubmitSourceBuffer(&buffer);
    if (FAILED(hr)) goto cleanup;

    hr = pSourceVoice->Start(0);
    if (FAILED(hr)) goto cleanup;

    // Wait for the audio to finish playing
    while (true) {
        XAUDIO2_VOICE_STATE state;
        pSourceVoice->GetState(&state);
        if (state.BuffersQueued == 0) {
            break;
        }
        Sleep(10); // Check every 10 milliseconds
    }

    std::cout << "Audio playback finished." << std::endl;

cleanup:;
}

void play() {
    HRESULT hr = pSourceVoice->Start(0);
}

void pause() {
    HRESULT hr = pSourceVoice->Stop(0);
}

void feedAudio() {
#define BUFSZ 44100
    static int curbuf = 0;
    static float buf[BUFSZ * 2 * 3];
    static std::vector<float> samples;
    static int lastSP = 0;
    static int playhead = 0;
    while(1){
        XAUDIO2_VOICE_STATE state;
        pSourceVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (state.BuffersQueued >= 2) break;
        float* cur = buf + curbuf * BUFSZ * 2;
        while (samples.size() < BUFSZ*2){
            DWORD streamFlags = 0;
            HRESULT hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &streamFlags, NULL, &pSample);
            if (FAILED(hr)) break;

            if (streamFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
                std::cout << "End of stream reached." << std::endl;
                break;
            }

            if (pSample) {
                // Convert to a contiguous buffer for easy access
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

        // Submit the buffer and start playback
        HRESULT hr = pSourceVoice->SubmitSourceBuffer(&xabuf);
        if (FAILED(hr)) break;

        curbuf = (curbuf + 1) % 3;
    }

    const int N_SAMPLES = 512;
    const int N_BINS = N_SAMPLES / 2 + 1;

    // Allocate input buffer for audio samples
    static float audio_in[N_SAMPLES];

    // Allocate output buffer for frequency bins
    kiss_fft_cpx frequency_out[N_BINS];

    XAUDIO2_VOICE_STATE state;
    pSourceVoice->GetState(&state);
    playhead = (playhead + state.SamplesPlayed - lastSP) % (3*BUFSZ);
    lastSP = state.SamplesPlayed;

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
    if (playhead < 0) playhead = 3 * BUFSZ + playhead;
    for (int i = 0; i < N_SAMPLES; i++) {
        audio_in[i] = (buf[((start + i) % (3 * BUFSZ)) * 2] + buf[((start + i) % (3 * BUFSZ)) * 2 + 1]) * 0.5f * hann[i];
    }

    // Allocate the configuration object
    kiss_fftr_cfg cfg = kiss_fftr_alloc(N_SAMPLES, 0, nullptr, nullptr);

    // Execute the FFT
    kiss_fftr(cfg, audio_in, frequency_out);

    static float target_frequencies[] = { 80.0f, 200.0f, 500.0f, 1200.0f, 3500.0f, 10000.0f };
    
    for (int i = 0; i < VISBARS; i++) {
        float target_freq = target_frequencies[i];
        int bin_index = static_cast<int>(std::round(target_freq * N_SAMPLES / 44100.0f));

        if (bin_index >= N_BINS) {
            // Handle cases where the target frequency is above the Nyquist frequency
            amplitudes[i] = 0.0f;
            continue;
        }

        float amplitude;
        if (bin_index == 0 || bin_index == N_SAMPLES / 2) {
            // DC or Nyquist bin
            amplitude = std::abs(frequency_out[bin_index].r) / N_SAMPLES;
        }
        else {
            amplitude = std::sqrt(frequency_out[bin_index].r * frequency_out[bin_index].r + frequency_out[bin_index].i * frequency_out[bin_index].i);
            amplitude =  2.0f * amplitude / N_SAMPLES;
        }
        amplitudes[i] = amplitude;
    };

    for (int i = 0; i < VISBARS; i++) {
        //std::cout << "Amplitude at " << target_frequencies[i] << "Hz: " << amplitudes[i] << std::endl;
    }

    // Free the configuration object
    kiss_fftr_free(cfg);
}

HRESULT loadSong(std::wstring input_file) {

    // Create the source reader from the MP3 file
    HRESULT hr = MFCreateSourceReaderFromURL(input_file.c_str(), NULL, &pReader);
    if (FAILED(hr)) {
        std::cerr << "Failed to create source reader for input file: " << std::endl;
        return hr;
    }

    IMFMediaType* pOutputMediaType = nullptr;

    // 1. Create a media type object.
    hr = MFCreateMediaType(&pOutputMediaType);

    // 2. Set the major type to audio.
    hr = pOutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);

    // 3. Set the subtype to 32-bit floating-point PCM.
    hr = pOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);

    // 4. Set the number of channels to stereo (2).
    hr = pOutputMediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);

    // 5. Set the sample rate to 44100 Hz.
    hr = pOutputMediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);

    // 6. Set the number of bits per sample (32 for float).
    hr = pOutputMediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32);

    // 7. Set the block alignment (channels * bytes_per_sample).
    hr = pOutputMediaType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2 * 4);

    // 8. Set the average bytes per second (sample_rate * block_alignment).
    hr = pOutputMediaType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 44100 * 2 * 4);

    // 9. Now, set this complete media type on the source reader.
    hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, pOutputMediaType);
    if (FAILED(hr)) {
        std::cerr << "Failed to set complete output format." << std::endl;
        return hr;
    }

    SafeRelease(&pOutputMediaType);

    return hr;
}