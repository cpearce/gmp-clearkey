#include <stdafx.h>

#define ENSURE_FUNCTION_PTR_HELPER(FunctionType, FunctionName, DLL) \
  static FunctionType FunctionName##Ptr = nullptr; \
  if (!FunctionName##Ptr) { \
    FunctionName##Ptr = (FunctionType) GetProcAddress(GetModuleHandleA(#DLL), #FunctionName); \
    if (!FunctionName##Ptr) { \
      return E_FAIL; \
    } \
  }

#define ENSURE_FUNCTION_PTR(FunctionName, DLL) \
  ENSURE_FUNCTION_PTR_HELPER(decltype(::FunctionName)*, FunctionName, DLL) \

#define ENSURE_FUNCTION_PTR_(FunctionName, DLL) \
  ENSURE_FUNCTION_PTR_HELPER(FunctionName##Ptr_t, FunctionName, DLL) \

#define DECL_FUNCTION_PTR(FunctionName, ...) \
  typedef HRESULT (STDMETHODCALLTYPE * FunctionName##Ptr_t)(__VA_ARGS__)

namespace wmf {

HRESULT
MFStartup()
{
  const int MF_VISTA_VERSION = (0x0001 << 16 | MF_API_VERSION);
  const int MF_WIN7_VERSION = (0x0002 << 16 | MF_API_VERSION);

  // decltype is unusable for functions having default parameters
  DECL_FUNCTION_PTR(MFStartup, ULONG, DWORD);
  ENSURE_FUNCTION_PTR_(MFStartup, Mfplat.dll)
  //if (!IsWin7OrLater())
  //  return MFStartupPtr(MF_VISTA_VERSION, MFSTARTUP_FULL);
  //else
  return MFStartupPtr(MF_WIN7_VERSION, MFSTARTUP_FULL);
}

HRESULT
MFShutdown()
{
  ENSURE_FUNCTION_PTR(MFShutdown, Mfplat.dll)
  return (MFShutdownPtr)();
}

HRESULT
MFGetStrideForBitmapInfoHeader(DWORD aFormat,
                               DWORD aWidth,
                               LONG *aOutStride)
{
  ENSURE_FUNCTION_PTR(MFGetStrideForBitmapInfoHeader, Mfplat.dll)
  return (MFGetStrideForBitmapInfoHeaderPtr)(aFormat, aWidth, aOutStride);
}

HRESULT
MFCreateSample(IMFSample **ppIMFSample)
{
  ENSURE_FUNCTION_PTR(MFCreateSample, mfplat.dll)
  return (MFCreateSamplePtr)(ppIMFSample);
}

HRESULT
MFCreateAlignedMemoryBuffer(DWORD cbMaxLength,
                            DWORD fAlignmentFlags,
                            IMFMediaBuffer **ppBuffer)
{
  ENSURE_FUNCTION_PTR(MFCreateAlignedMemoryBuffer, mfplat.dll)
  return (MFCreateAlignedMemoryBufferPtr)(cbMaxLength, fAlignmentFlags, ppBuffer);
}

HRESULT
MFCreateMediaType(IMFMediaType **aOutMFType)
{
  ENSURE_FUNCTION_PTR(MFCreateMediaType, Mfplat.dll)
  return (MFCreateMediaTypePtr)(aOutMFType);
}

} // namespace wmf