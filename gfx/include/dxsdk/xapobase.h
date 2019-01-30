/*-========================================================================-_
 |                                 - XAPO -                                 |
 |        Copyright (c) Microsoft Corporation.  All rights reserved.        |
 |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|
 |PROJECT: XAPO                         MODEL:   Unmanaged User-mode        |
 |VERSION: 1.0                          EXCEPT:  No Exceptions              |
 |CLASS:   N / A                        MINREQ:  WinXP, Xbox360             |
 |BASE:    N / A                        DIALECT: MSC++ 14.00                |
 |>------------------------------------------------------------------------<|
 | DUTY: XAPO base classes                                                  |
 ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~^
  NOTES:
    1.  See XAPO.h for the rules governing XAPO interface behaviour.        */

#ifdef _MSC_VER
#pragma once
#endif

//--------------<D-E-F-I-N-I-T-I-O-N-S>-------------------------------------//
#include "XAPO.h"

// default audio format ranges supported, applies to XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS.pFormat
#define XAPOBASE_DEFAULT_FORMAT_TAG           WAVE_FORMAT_IEEE_FLOAT // 32-bit float only, applies to WAVEFORMATEX.wFormatTag or WAVEFORMATEXTENSIBLE.SubFormat when used
#define XAPOBASE_DEFAULT_FORMAT_MIN_CHANNELS  XAPO_MIN_CHANNELS      // minimum channel count, applies to WAVEFORMATEX.nChannels
#define XAPOBASE_DEFAULT_FORMAT_MAX_CHANNELS  XAPO_MAX_CHANNELS      // maximum channel count, applies to WAVEFORMATEX.nChannels
#define XAPOBASE_DEFAULT_FORMAT_MIN_FRAMERATE XAPO_MIN_FRAMERATE     // minimum framerate, applies to WAVEFORMATEX.nSamplesPerSec
#define XAPOBASE_DEFAULT_FORMAT_MAX_FRAMERATE XAPO_MAX_FRAMERATE     // maximum framerate, applies to WAVEFORMATEX.nSamplesPerSec
#define XAPOBASE_DEFAULT_FORMAT_BITSPERSAMPLE 32                     // 32-bit float only, applies to WAVEFORMATEX.wBitsPerSample and WAVEFORMATEXTENSIBLE.wValidBitsPerSample when used

// default XAPO property flags supported, applies to XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS
#define XAPOBASE_DEFAULT_FLAG (XAPO_FLAG_CHANNELS_MUST_MATCH | XAPO_FLAG_FRAMERATE_MUST_MATCH | XAPO_FLAG_BITSPERSAMPLE_MUST_MATCH | XAPO_FLAG_BUFFERCOUNT_MUST_MATCH | XAPO_FLAG_INPLACE_SUPPORTED)

// default number of input and output buffers supported, applies to XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS
#define XAPOBASE_DEFAULT_BUFFER_COUNT 1

//--------------<M-A-C-R-O-S>-----------------------------------------------//
// assertion
#if !defined(XAPOASSERT)
    #if XAPODEBUG
        #define XAPOASSERT(exp) if (!(exp)) { OutputDebugStringA("XAPO ASSERT: " #exp ", {" __FUNCTION__ "}\n"); __debugbreak(); }
    #else
        #define XAPOASSERT(exp) __assume(exp)
    #endif
#endif
#if !defined(XAPOASSERT_NO_OUTPUT)
    #if XAPODEBUG
        #define XAPOASSERT_NO_OUTPUT(exp) if (!(exp)) { __debugbreak(); }
    #else
        #define XAPOASSERT_NO_OUTPUT(exp) __assume(exp)
    #endif
#endif

//--------------<D-A-T-A---T-Y-P-E-S>---------------------------------------//
#pragma pack(push, 8) // set packing alignment to ensure consistency across arbitrary build environments, and ensure synchronization variables used by Interlocked functionality are correctly aligned

// primitive types
typedef float FLOAT32; // 32-bit IEEE float

  ////
  // DESCRIPTION:
  //  Default implementation of the IXAPO and IUnknown interfaces.
  //  Provides overridable implementations for all methods save IXAPO::Process.
  ////
class __declspec(novtable) CXAPOBase: public IXAPO {
private:
    const XAPO_REGISTRATION_PROPERTIES* m_pRegistrationProperties; // pointer to registration properties of the XAPO, set via constructor

    void*    m_pfnMatrixMixFunction;    // optimal matrix function pointer, used for thru processing
    FLOAT32* m_pfl32MatrixCoefficients; // matrix coefficient table, used for thru processing
    UINT32   m_nSrcFormatType;          // input format type, used for thru processing
    BOOL     m_fIsScalarMatrix;         // TRUE if m_pfl32MatrixCoefficients is diagonal matrix with all main diagonal entries equal, i.e. m_pfnMatrixMixFunction only used for type conversion (no channel conversion), used for thru processing
    BOOL     m_fIsLocked;               // TRUE if XAPO locked via CXAPOBase.LockForProcess

protected:
    LONG m_lReferenceCount; // COM reference count, must be aligned for atomic operations

      ////
      // DESCRIPTION:
      //  Verifies an audio format falls within the default ranges supported.
      //
      // REMARKS:
      //  If pFormat is unsupported, and fOverwrite is TRUE,
      //  pFormat is overwritten with the nearest format supported.
      //  Nearest meaning closest bit depth, framerate, and channel count,
      //  in that order of importance.
      //
      // PARAMETERS:
      //  pFormat    - [in/out] audio format to examine
      //  fOverwrite - [in]     TRUE to overwrite pFormat if audio format unsupported
      //
      // RETURN VALUE:
      //  COM error code, including:
      //    S_OK                      - audio format supported, pFormat left untouched
      //    XAPO_E_FORMAT_UNSUPPORTED - audio format unsupported, pFormat overwritten with nearest audio format supported if fOverwrite TRUE
      //    E_INVALIDARG              - audio format invalid, pFormat left untouched
      ////
    virtual HRESULT ValidateFormatDefault (_Inout_ WAVEFORMATEX* pFormat, BOOL fOverwrite);

      ////
      // DESCRIPTION:
      //  Verifies that an input/output format pair configuration is supported
      //  with respect to the XAPO property flags.
      //
      // REMARKS:
      //  If pRequestedFormat is unsupported, and fOverwrite is TRUE,
      //  pRequestedFormat is overwritten with the nearest format supported.
      //  Nearest meaning closest bit depth, framerate, and channel count,
      //  in that order of importance.
      //
      // PARAMETERS:
      //  pSupportedFormat - [in]     audio format known to be supported
      //  pRequestedFormat - [in/out] audio format to examine, must be WAVEFORMATEXTENSIBLE if fOverwrite TRUE
      //  fOverwrite       - [in]     TRUE to overwrite pRequestedFormat if input/output configuration unsupported
      //
      // RETURN VALUE:
      //  COM error code, including:
      //    S_OK                      - input/output configuration supported, pRequestedFormat left untouched
      //    XAPO_E_FORMAT_UNSUPPORTED - input/output configuration unsupported, pRequestedFormat overwritten with nearest audio format supported if fOverwrite TRUE
      //    E_INVALIDARG              - either audio format invalid, pRequestedFormat left untouched
      ////
    HRESULT ValidateFormatPair (const WAVEFORMATEX* pSupportedFormat, _Inout_ WAVEFORMATEX* pRequestedFormat, BOOL fOverwrite);

      ////
      // DESCRIPTION:
      //  This method may be called by an IXAPO::Process implementation
      //  for thru processing.  It copies/mixes data from source to
      //  destination, making as few changes as possible to the audio data.
      //
      // REMARKS:
      //  However, this method is capable of channel upmix/downmix and uses
      //  the same matrix coefficient table used by windows Vista to do so.
      //
      //  For in-place processing (input buffer == output buffer)
      //  this method does nothing.
      //
      //  This method should be called only if the XAPO is locked and
      //  XAPO_FLAG_FRAMERATE_MUST_MATCH is used.
      //
      // PARAMETERS:
      //  pInputBuffer       - [in]  input buffer, format may be INT8, INT16, INT20 (contained in 24 or 32 bits), INT24 (contained in 24 or 32 bits), INT32, or FLOAT32
      //  pOutputBuffer      - [out] output buffer, format must be FLOAT32
      //  FrameCount         - [in]  number of frames to process
      //  InputChannelCount  - [in]  number of input channels
      //  OutputChannelCount - [in]  number of output channels
      //  MixWithOutput      - [in]  TRUE to mix with output, FALSE to overwrite output
      //
      // RETURN VALUE:
      //  void
      ////
    void ProcessThru (const void* pInputBuffer, _Inout_updates_(FrameCount*OutputChannelCount) FLOAT32* pOutputBuffer, UINT32 FrameCount, UINT32 InputChannelCount, UINT32 OutputChannelCount, BOOL MixWithOutput);

    // accessors
    const XAPO_REGISTRATION_PROPERTIES* GetRegistrationPropertiesInternal () { return m_pRegistrationProperties; }
    BOOL IsLocked  () { return m_fIsLocked; }

public:
    CXAPOBase (const XAPO_REGISTRATION_PROPERTIES* pRegistrationProperties);
    virtual ~CXAPOBase ();

    // IUnknown methods:
    // retrieves the requested interface pointer if supported
    STDMETHOD(QueryInterface) (REFIID riid, _Outptr_ void** ppInterface)
    {
        XAPOASSERT(ppInterface != NULL);
        HRESULT hr = S_OK;

        if (riid == __uuidof(IXAPO)) {
            *ppInterface = static_cast<IXAPO*>(this);
            AddRef();
        } else if (riid == __uuidof(IUnknown)) {
            *ppInterface = static_cast<IUnknown*>(this);
            AddRef();
        } else {
            *ppInterface = NULL;
            hr = E_NOINTERFACE;
        }

        return hr;
    }

    // increments reference count
    STDMETHOD_(ULONG, AddRef) ()
    {
        return (ULONG)InterlockedIncrement(&m_lReferenceCount);
    }

    // decrements reference count and deletes the object if the reference count falls to zero
    STDMETHOD_(ULONG, Release) ()
    {
        ULONG uTmpReferenceCount = (ULONG)InterlockedDecrement(&m_lReferenceCount);
        if (uTmpReferenceCount == 0) {
            delete this;
        }
        return uTmpReferenceCount;
    }

    // IXAPO methods:
    // Allocates a copy of the registration properties of the XAPO.
    // This default implementation returns a copy of the registration
    // properties given to the constructor, allocated via XAPOAlloc.
    STDMETHOD(GetRegistrationProperties) (_Outptr_ XAPO_REGISTRATION_PROPERTIES** ppRegistrationProperties);

    // Queries if a specific input format is supported for a given output format.
    // This default implementation assumes only the format described by the
    // XAPOBASE_DEFAULT_FORMAT values are supported for both input and output.
    STDMETHOD(IsInputFormatSupported) (const WAVEFORMATEX* pOutputFormat, const WAVEFORMATEX* pRequestedInputFormat, _Outptr_opt_ WAVEFORMATEX** ppSupportedInputFormat);

    // Queries if a specific output format is supported for a given input format.
    // This default implementation assumes only the format described by the
    // XAPOBASE_DEFAULT_FORMAT values are supported for both input and output.
    STDMETHOD(IsOutputFormatSupported) (const WAVEFORMATEX* pInputFormat, const WAVEFORMATEX* pRequestedOutputFormat, _Outptr_opt_ WAVEFORMATEX** ppSupportedOutputFormat);

    // Performs any effect-specific initialization.
    // This default implementation is a no-op and only returns S_OK.
    STDMETHOD(Initialize) (_In_reads_bytes_opt_(DataByteSize) const void*, UINT32 DataByteSize)
    {
        UNREFERENCED_PARAMETER(DataByteSize);
        return S_OK;
    }

    // Resets variables dependent on frame history.
    // This default implementation is a no-op: this base class contains no
    // relevant state to reset.
    STDMETHOD_(void, Reset) () { return; }

    // Notifies XAPO of buffer formats Process() will be given.
    // This default implementation performs basic input/output format
    // validation against the XAPO's registration properties.
    // Derived XAPOs should call the base implementation first.
    STDMETHOD(LockForProcess) (_Pre_equal_to_(OutputLockedParameterCount) UINT32 InputLockedParameterCount, _In_reads_opt_(InputLockedParameterCount) const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pInputLockedParameters, _Pre_equal_to_(InputLockedParameterCount) UINT32 OutputLockedParameterCount, _In_reads_opt_(OutputLockedParameterCount) const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pOutputLockedParameters);

    // Opposite of LockForProcess.
    // Derived XAPOs should call the base implementation first.
    STDMETHOD_(void, UnlockForProcess) ();

    // Returns the number of input frames required to generate the requested number of output frames.
    // By default, this method returns the same number of frames it was passed.
    STDMETHOD_(UINT32, CalcInputFrames) (UINT32 OutputFrameCount) { return OutputFrameCount; }

    // Returns the number of output frames generated for the requested number of input frames.
    // By default, this method returns the same number of frames it was passed.
    STDMETHOD_(UINT32, CalcOutputFrames) (UINT32 InputFrameCount) { return InputFrameCount; }
};

//--------------------------------------------------------------------------//
  ////
  // DESCRIPTION:
  //  Extends CXAPOBase, providing a default implementation of the
  //  IXAPOParameters interface with appropriate synchronization to
  //  protect variables shared between IXAPOParameters::GetParameters
  //  and IXAPOParameters::SetParameters/IXAPO::Process.
  //
  //  This class is for parameter blocks whose size is larger than 4 bytes.
  //  For smaller parameter blocks, use atomic operations directly
  //  on the parameters for synchronization.
  ////
class __declspec(novtable) CXAPOParametersBase: public CXAPOBase, public IXAPOParameters {
private:
    BYTE*  m_pParameterBlocks;           // three contiguous process parameter blocks used for synchronization, user responsible for initialization of parameter blocks before IXAPO::Process/SetParameters/GetParameters called
    BYTE*  m_pCurrentParameters;         // pointer to current process parameters, must be aligned for atomic operations
    BYTE*  m_pCurrentParametersInternal; // pointer to current process parameters (temp pointer read by SetParameters/BeginProcess/EndProcess)
    UINT32 m_uCurrentParametersIndex;    // index of current process parameters
    UINT32 m_uParameterBlockByteSize;    // size of a single parameter block in bytes, must be > 0
    BOOL   m_fNewerResultsReady;         // TRUE if there exists new processing results not yet picked up by GetParameters(), must be aligned for atomic operations
    BOOL   m_fProducer;                  // TRUE if IXAPO::Process produces data to be returned by GetParameters(), SetParameters() and ParametersChanged() disallowed

public:
      ////
      // PARAMETERS:
      //  pRegistrationProperties - [in] registration properties of the XAPO
      //  pParameterBlocks        - [in] three contiguous process parameter blocks used for synchronization
      //  uParameterBlockByteSize - [in] size of one of the parameter blocks, must be > 0, should be > 4
      //  fProducer               - [in] TRUE if IXAPO::Process produces data to be returned by GetParameters() (SetParameters() and ParametersChanged() disallowed)
      ////
    CXAPOParametersBase (const XAPO_REGISTRATION_PROPERTIES* pRegistrationProperties, _In_reads_bytes_opt_(3*uParameterBlockByteSize) BYTE* pParameterBlocks, UINT32 uParameterBlockByteSize, BOOL fProducer);
    virtual ~CXAPOParametersBase ();

    // IUnknown methods:
    // retrieves the requested interface pointer if supported
    STDMETHOD(QueryInterface) (REFIID riid, _Outptr_result_maybenull_ void** ppInterface)
    {
        XAPOASSERT(ppInterface != NULL);
        HRESULT hr = S_OK;

        if (riid == __uuidof(IXAPOParameters)) {
            *ppInterface = static_cast<IXAPOParameters*>(this);
            CXAPOBase::AddRef();
        } else {
            hr = CXAPOBase::QueryInterface(riid, ppInterface);
        }

        return hr;
    }

    // increments reference count
    STDMETHOD_(ULONG, AddRef)() { return CXAPOBase::AddRef(); }

    // decrements reference count and deletes the object if the reference count falls to zero
    STDMETHOD_(ULONG, Release)() { return CXAPOBase::Release(); }

    // IXAPOParameters methods:
    // Sets effect-specific parameters.
    // This method may only be called on the realtime audio processing thread.
    STDMETHOD_(void, SetParameters) (_In_reads_bytes_(ParameterByteSize) const void* pParameters, UINT32 ParameterByteSize);

    // Gets effect-specific parameters.
    // This method may block and should not be called from the realtime thread.
    // Get the current parameters via BeginProcess.
    STDMETHOD_(void, GetParameters) (_Out_writes_bytes_(ParameterByteSize) void* pParameters, UINT32 ParameterByteSize);

    // Called by SetParameters() to allow for user-defined parameter validation.
    // SetParameters validates that ParameterByteSize == m_uParameterBlockByteSize
    // so the user may assume/assert ParameterByteSize == m_uParameterBlockByteSize.
    // This method should not block as it is called from the realtime thread.
    virtual void OnSetParameters (_In_reads_bytes_(ParameterByteSize) const void* pParameters, UINT32 ParameterByteSize)
    {
        XAPOASSERT(m_uParameterBlockByteSize > 0);
        XAPOASSERT(pParameters != NULL);
        XAPOASSERT(ParameterByteSize == m_uParameterBlockByteSize);
    }

    // Returns TRUE if SetParameters() has been called since the last processing pass.
    // May only be used within the XAPO's IXAPO::Process implementation,
    // before BeginProcess is called.
    BOOL ParametersChanged ();

    // Returns latest process parameters.
    // XAPOs must call this method within their IXAPO::Process
    // implementation to access latest process parameters in threadsafe manner.
    BYTE* BeginProcess ();

    // Notifies CXAPOParametersBase that the XAPO has finished accessing
    // the latest process parameters.
    // XAPOs must call this method within their IXAPO::Process
    // implementation to access latest process parameters in threadsafe manner.
    void EndProcess ();
};

#pragma pack(pop) // revert packing alignment
//---------------------------------<-EOF->----------------------------------//
