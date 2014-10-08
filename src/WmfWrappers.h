namespace wmf {

HRESULT
MFStartup();

HRESULT
MFShutdown();

HRESULT
MFGetStrideForBitmapInfoHeader(DWORD aFormat,
                               DWORD aWidth,
                               LONG *aOutStride);

HRESULT
MFCreateSample(IMFSample **ppIMFSample);

HRESULT
MFCreateAlignedMemoryBuffer(DWORD cbMaxLength,
                            DWORD fAlignmentFlags,
                            IMFMediaBuffer **ppBuffer);

HRESULT
MFCreateMediaType(IMFMediaType **aOutMFType);

} // namespace wmf