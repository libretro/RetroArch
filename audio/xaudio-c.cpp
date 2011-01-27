/*
	Simple C interface for XAudio2
	Author: Hans-Kristian Arntzen
	License: Public Domain
*/

// This will currently only compile with MSVC2k10.
// Should be compilable with some MinGW trickery as well.

#include "xaudio-c.h"

struct xaudio2 : public IXAudio2VoiceCallback
{
	xaudio2(unsigned samplerate, unsigned channels, unsigned bits, unsigned size) : 
		buf(nullptr), pXAudio2(nullptr), pMasterVoice(nullptr), pSourceVoice(nullptr),
			buffers(0), bufptr(0)
	{
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		HRESULT hr;
		if (FAILED(hr = XAudio2Create(&pXAudio2, 0)))
			throw -1;
		if (FAILED(hr = pXAudio2->CreateMasteringVoice(
			&pMasterVoice, channels, samplerate, 0, 0, nullptr)))
			throw -1;
		WAVEFORMATEX wfx;
		wfx.wFormatTag = WAVE_FORMAT_PCM;
		wfx.nChannels = channels;
		wfx.nSamplesPerSec = samplerate;
		wfx.nBlockAlign = channels * bits / 8;
		wfx.wBitsPerSample = bits;
		wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
		wfx.cbSize = 0;
		if (FAILED(hr = pXAudio2->CreateSourceVoice(
			&pSourceVoice, &wfx, XAUDIO2_VOICE_NOSRC, XAUDIO2_DEFAULT_FREQ_RATIO, this, nullptr, nullptr)))
			throw -1;

		pSourceVoice->Start();

		hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!hEvent)
			throw -1;

		max_buffers = 8;
		bufsize = size / max_buffers;
		buf = new uint8_t[bufsize * max_buffers];
		write_buffer = 0;
	}

	~xaudio2()
	{
		if (pSourceVoice)
		{
			pSourceVoice->Stop();
			pSourceVoice->DestroyVoice();
		}

		if (pMasterVoice)
			pMasterVoice->DestroyVoice();

		if (pXAudio2)
			pXAudio2->Release();

		if (hEvent)
			CloseHandle(hEvent);

		if (buf)
			delete[] buf;
	}

	size_t write_avail() const
	{
		return bufsize * (max_buffers - buffers - 1);
	}
	
	size_t write(const void *buffer, size_t bytes)
	{
		size_t written = 0;
		while (written < bytes)
		{
			size_t need = min(bytes - written, bufsize - bufptr);
			memcpy((uint8_t*)buf + write_buffer * bufsize + bufptr, (const uint8_t*)buffer + written, need);
			written += need;
			bufptr += need;

			if (bufptr == bufsize)
			{
				while (buffers == max_buffers - 1)
					WaitForSingleObject(hEvent, INFINITE);

				XAUDIO2_BUFFER xa2buffer = {0};
				xa2buffer.AudioBytes = bufsize;
				xa2buffer.pAudioData = buf + write_buffer * bufsize;
				xa2buffer.pContext = nullptr;
				InterlockedIncrement(&buffers);
				pSourceVoice->SubmitSourceBuffer(&xa2buffer);
				bufptr = 0;
				write_buffer = (write_buffer + 1) % max_buffers;
			}
		}
		return bytes;
	}

	STDMETHOD_(void, OnBufferStart) (void *) {}
	STDMETHOD_(void, OnBufferEnd) (void *) 
	{
		InterlockedDecrement(&buffers);
		SetEvent(hEvent);
	}
   STDMETHOD_(void, OnLoopEnd) (void *) {}
   STDMETHOD_(void, OnStreamEnd) () {}
	STDMETHOD_(void, OnVoiceError) (void *, HRESULT) {}
	STDMETHOD_(void, OnVoiceProcessingPassEnd) () 
	{
		SetEvent(hEvent);
	}
	STDMETHOD_(void, OnVoiceProcessingPassStart) (UINT32) {}

	uint8_t *buf;
	IXAudio2 *pXAudio2;
	IXAudio2MasteringVoice *pMasterVoice;
	IXAudio2SourceVoice *pSourceVoice;
	HANDLE hEvent;

	volatile long buffers;
	unsigned max_buffers;
	unsigned bufsize;
	unsigned bufptr;
	unsigned write_buffer;
};

XAUDIOC_API xaudio2_t *xaudio2_new(unsigned samplerate, unsigned channels, unsigned bits, size_t size)
{
	xaudio2 *handle = nullptr;
	try {
		handle = new xaudio2(samplerate, channels, bits, size);
	} catch(...) { delete handle; return nullptr; }

	return handle;
}

XAUDIOC_API size_t xaudio2_write_avail(xaudio2_t *handle)
{
	try {
		return handle->write_avail();
	} catch(...) { return 0; }
}

XAUDIOC_API size_t xaudio2_write(xaudio2_t *handle, const void *buf, size_t bytes)
{
	try {
		return handle->write(buf, bytes);
	} catch(...) { return 0; }
}

XAUDIOC_API void xaudio2_free(xaudio2_t *handle)
{
	delete handle;
}


