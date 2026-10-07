#include "AudioManager.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <vector>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

void AudioManager::Initialize() {
	HRESULT result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	assert(SUCCEEDED(result));

	result = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(result));

	result = xAudio2_->CreateMasteringVoice(&masterVoice_);
	assert(SUCCEEDED(result));
}

void AudioManager::Finalize() {
	if (masterVoice_) {
		masterVoice_->DestroyVoice();
		masterVoice_ = nullptr;
	}
	xAudio2_.Reset();

	const HRESULT result = MFShutdown();
	assert(SUCCEEDED(result));
}

AudioManager::SoundData AudioManager::Load(const std::string& filename) {
	const int wideLength = MultiByteToWideChar(CP_UTF8, 0, filename.c_str(), -1,
	                                           nullptr, 0);
	assert(wideLength > 0);
	std::wstring wideFilename(static_cast<size_t>(wideLength), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, filename.c_str(), -1, wideFilename.data(),
	                    wideLength);

	Microsoft::WRL::ComPtr<IMFSourceReader> reader;
	HRESULT result = MFCreateSourceReaderFromURL(wideFilename.c_str(), nullptr,
	                                             &reader);
	assert(SUCCEEDED(result));

	Microsoft::WRL::ComPtr<IMFMediaType> outputType;
	result = MFCreateMediaType(&outputType);
	assert(SUCCEEDED(result));
	result = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	assert(SUCCEEDED(result));
	result = outputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	assert(SUCCEEDED(result));
	result = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,
	                                     nullptr, outputType.Get());
	assert(SUCCEEDED(result));

	Microsoft::WRL::ComPtr<IMFMediaType> currentType;
	result = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,
	                                     &currentType);
	assert(SUCCEEDED(result));

	WAVEFORMATEX* waveFormat = nullptr;
	UINT32 waveFormatSize = 0;
	result = MFCreateWaveFormatExFromMFMediaType(
	    currentType.Get(), &waveFormat, &waveFormatSize,
	    MFWaveFormatExConvertFlag_Normal);
	assert(SUCCEEDED(result));
	assert(waveFormatSize >= sizeof(WAVEFORMATEX));

	std::vector<BYTE> audioBytes;
	while (true) {
		DWORD flags = 0;
		Microsoft::WRL::ComPtr<IMFSample> sample;
		result = reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0,
		                            nullptr, &flags, nullptr, &sample);
		assert(SUCCEEDED(result));
		if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
			break;
		}
		if (!sample) {
			continue;
		}

		Microsoft::WRL::ComPtr<IMFMediaBuffer> mediaBuffer;
		result = sample->ConvertToContiguousBuffer(&mediaBuffer);
		assert(SUCCEEDED(result));

		BYTE* data = nullptr;
		DWORD dataSize = 0;
		result = mediaBuffer->Lock(&data, nullptr, &dataSize);
		assert(SUCCEEDED(result));
		audioBytes.insert(audioBytes.end(), data, data + dataSize);
		mediaBuffer->Unlock();
	}

	SoundData soundData{};
	soundData.wfex = *waveFormat;
	CoTaskMemFree(waveFormat);
	soundData.bufferSize = static_cast<unsigned int>(audioBytes.size());
	soundData.pBuffer = new BYTE[soundData.bufferSize];
	std::memcpy(soundData.pBuffer, audioBytes.data(), soundData.bufferSize);
	return soundData;
}

void AudioManager::Unload(SoundData* soundData) {
	assert(soundData);
	delete[] soundData->pBuffer;
	soundData->pBuffer = nullptr;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

IXAudio2SourceVoice* AudioManager::Play(const SoundData& soundData, bool loop,
	                                    float volume) {
	IXAudio2SourceVoice* sourceVoice = nullptr;
	HRESULT result = xAudio2_->CreateSourceVoice(&sourceVoice, &soundData.wfex);
	assert(SUCCEEDED(result));
	result = sourceVoice->SetVolume(volume);
	assert(SUCCEEDED(result));

	XAUDIO2_BUFFER buffer{};
	buffer.pAudioData = soundData.pBuffer;
	buffer.AudioBytes = soundData.bufferSize;
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

	result = sourceVoice->SubmitSourceBuffer(&buffer);
	assert(SUCCEEDED(result));
	result = sourceVoice->Start();
	assert(SUCCEEDED(result));
	return sourceVoice;
}

void AudioManager::Stop(IXAudio2SourceVoice*& sourceVoice) {
	if (!sourceVoice) {
		return;
	}
	sourceVoice->Stop();
	sourceVoice->FlushSourceBuffers();
	sourceVoice->DestroyVoice();
	sourceVoice = nullptr;
}
