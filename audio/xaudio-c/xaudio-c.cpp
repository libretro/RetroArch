/*
	Simple C interface for XAudio2
	Author: Hans-Kristian Arntzen
	License: Public Domain
*/

#include "xaudio-c.h"
#include "xaudio2.hpp"
#include <stdint.h>
#include <algorithm>

struct xaudio2 : public IXAudio2VoiceCallback
{
	xaudio2(unsigned samplerate, unsigned channels, unsigned bits, unsigned size) : 
		buf(0), pXAudio2(0), pMasterVoice(0), pSourceVoice(0),
			buffers(0), bufptr(0)
	{
		CoInitializeEx(0, COINIT_MULTITHREADED);
		HRESULT hr;
		if (FAILED(hr = XAudio2Create(&pXAudio2, 0)))
			throw -1;
		if (FAILED(hr = pXAudio2->CreateMasteringVoice(
			&pMasterVoice, channels, samplerate, 0, 0, 0)))
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
			&pSourceVoice, &wfx, XAUDIO2_VOICE_NOSRC, XAUDIO2_DEFAULT_FREQ_RATIO, this, 0, 0)))
			throw -1;

		hEvent = CreateEvent(0, FALSE, FALSE, 0);
		if (!hEvent)
			throw -1;

		pSourceVoice->Start(0);

		max_buffers = 8;
		bufsize = size / max_buffers;
		buf = new uint8_t[bufsize * max_buffers];
		write_buffer = 0;
	}

	virtual ~xaudio2()
	{
		if (pSourceVoice)
		{
			pSourceVoice->Stop(0);
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
			size_t need = std::min(bytes - written, static_cast<size_t>(bufsize - bufptr));
			memcpy(buf + write_buffer * bufsize + bufptr, (const uint8_t*)buffer + written, need);
			written += need;
			bufptr += need;

			if (bufptr == bufsize)
			{
				while (static_cast<volatile unsigned>(buffers) == max_buffers - 1)
					WaitForSingleObject(hEvent, INFINITE);

				XAUDIO2_BUFFER xa2buffer = {0};
				xa2buffer.AudioBytes = bufsize;
				xa2buffer.pAudioData = buf + write_buffer * bufsize;
				xa2buffer.pContext = 0;
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

xaudio2_t *xaudio2_new(unsigned samplerate, unsigned channels, unsigned bits, size_t size)
{
	xaudio2 *handle = 0;
	try {
		handle = new xaudio2(samplerate, channels, bits, size);
	} catch(...) { delete handle; return 0; }

	return handle;
}

size_t xaudio2_write_avail(xaudio2_t *handle)
{
	try {
		return handle->write_avail();
	} catch(...) { return 0; }
}

size_t xaudio2_write(xaudio2_t *handle, const void *buf, size_t bytes)
{
	try {
		return handle->write(buf, bytes);
	} catch(...) { return 0; }
}

void xaudio2_free(xaudio2_t *handle)
{
	delete handle;
}

