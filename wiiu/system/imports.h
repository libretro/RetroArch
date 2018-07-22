/* coreinit */
IMPORT_BEGIN(coreinit);

IMPORT(OSScreenInit);
IMPORT(OSScreenGetBufferSizeEx);
IMPORT(OSScreenSetBufferEx);
IMPORT(OSScreenEnableEx);
IMPORT(OSScreenFlipBuffersEx);
IMPORT(OSScreenClearBufferEx);
IMPORT(OSScreenPutFontEx);
IMPORT(OSFatal);
IMPORT(OSDynLoad_Acquire);
IMPORT(OSDynLoad_FindExport);
IMPORT(OSDynLoad_Release);
IMPORT(OSSetExceptionCallback);
IMPORT(OSSavesDone_ReadyToRelease);
IMPORT(OSInitMutex);
IMPORT(OSLockMutex);
IMPORT(OSUnlockMutex);
IMPORT(OSInitCond);
IMPORT(OSWaitCond);
IMPORT(OSSignalCond);
IMPORT(OSInitSpinLock);
IMPORT(OSUninterruptibleSpinLock_Acquire);
IMPORT(OSUninterruptibleSpinLock_Release);
IMPORT(OSFastMutex_Init);
IMPORT(OSFastMutex_Lock);
IMPORT(OSFastMutex_Unlock);
IMPORT(OSSleepTicks);
IMPORT(OSGetTitleID);
IMPORT(OSIsThreadTerminated);
IMPORT(OSSetThreadPriority);
IMPORT(OSCreateThread);
IMPORT(OSSetThreadCleanupCallback);
IMPORT(OSResumeThread);
IMPORT(OSIsThreadSuspended);
IMPORT(OSSuspendThread);
IMPORT(OSGetCurrentThread);
IMPORT(OSExitThread);
IMPORT(OSJoinThread);
IMPORT(OSYieldThread);
IMPORT(OSGetCoreId);
IMPORT(OSIsMainCore);
IMPORT(OSGetSystemTime);
IMPORT(OSGetSystemTick);
IMPORT(OSGetTime);
IMPORT(OSGetSymbolName);
IMPORT(OSGetSharedData);
IMPORT(OSEffectiveToPhysical);
IMPORT(OSInitSemaphore);
IMPORT(OSInitSemaphoreEx);
IMPORT(OSGetSemaphoreCount);
IMPORT(OSSignalSemaphore);
IMPORT(OSWaitSemaphore);
IMPORT(OSTryWaitSemaphore);

IMPORT(exit);
IMPORT(_Exit);
IMPORT(__os_snprintf);
IMPORT(DisassemblePPCRange);

IMPORT(DCInvalidateRange);
IMPORT(DCFlushRange);
IMPORT(DCStoreRange);
IMPORT(DCStoreRangeNoSync);

IMPORT(__gh_errno_ptr);

IMPORT(MEMGetBaseHeapHandle);
IMPORT(MEMCreateExpHeapEx);
IMPORT(MEMDestroyExpHeap);
IMPORT(MEMAllocFromExpHeapEx);
IMPORT(MEMFreeToExpHeap);
IMPORT(MEMGetSizeForMBlockExpHeap);
IMPORT(MEMAllocFromFrmHeapEx);
IMPORT(MEMFreeToFrmHeap);
IMPORT(MEMGetAllocatableSizeForFrmHeapEx);

IMPORT(FSInit);
IMPORT(FSShutdown);
IMPORT(FSAddClient);
IMPORT(FSAddClientEx);
IMPORT(FSDelClient);
IMPORT(FSInitCmdBlock);
IMPORT(FSChangeDir);
IMPORT(FSGetFreeSpaceSize);
IMPORT(FSGetStat);
IMPORT(FSRemove);
IMPORT(FSOpenFile);
IMPORT(FSCloseFile);
IMPORT(FSOpenDir);
IMPORT(FSMakeDir);
IMPORT(FSReadDir);
IMPORT(FSRewindDir);
IMPORT(FSCloseDir);
IMPORT(FSGetStatFile);
IMPORT(FSReadFile);
IMPORT(FSWriteFile);
IMPORT(FSSetPosFile);
IMPORT(FSFlushFile);
IMPORT(FSTruncateFile);
IMPORT(FSRename);
IMPORT(FSGetMountSource);
IMPORT(FSMount);
IMPORT(FSUnmount);

IMPORT(IOS_Open);
IMPORT(IOS_Close);
IMPORT(IOS_Ioctl);
IMPORT(IOS_IoctlAsync);

IMPORT(IMIsAPDEnabled);
IMPORT(IMIsDimEnabled);
IMPORT(IMEnableAPD);
IMPORT(IMEnableDim);
IMPORT(IMDisableAPD);
IMPORT(IMDisableDim);

IMPORT_END();

/* nsysnet */
IMPORT_BEGIN(nsysnet);

IMPORT(socket_lib_init);
IMPORT(getaddrinfo);
IMPORT(freeaddrinfo);
IMPORT(getnameinfo);
IMPORT(inet_ntoa);
IMPORT(inet_ntop);
IMPORT(inet_aton);
IMPORT(inet_pton);
IMPORT(ntohl);
IMPORT(ntohs);
IMPORT(htonl);
IMPORT(htons);
IMPORT(accept);
IMPORT(bind);
IMPORT(socketclose);
IMPORT(connect);
IMPORT(getpeername);
IMPORT(getsockname);
IMPORT(getsockopt);
IMPORT(listen);
IMPORT(recv);
IMPORT(recvfrom);
IMPORT(send);
IMPORT(sendto);
IMPORT(setsockopt);
IMPORT(shutdown);
IMPORT(socket);
IMPORT(select);
IMPORT(socketlasterr);

IMPORT_END();

/* gx2 */
IMPORT_BEGIN(gx2);

IMPORT(GX2Invalidate);
IMPORT(GX2Init);
IMPORT(GX2GetSystemTVScanMode);
IMPORT(GX2CalcTVSize);
IMPORT(GX2SetTVBuffer);
IMPORT(GX2CalcDRCSize);
IMPORT(GX2SetDRCBuffer);
IMPORT(GX2CalcSurfaceSizeAndAlignment);
IMPORT(GX2InitColorBufferRegs);
IMPORT(GX2SetupContextStateEx);
IMPORT(GX2SetContextState);
IMPORT(GX2SetColorBuffer);
IMPORT(GX2SetViewport);
IMPORT(GX2SetScissor);
IMPORT(GX2SetDepthOnlyControl);
IMPORT(GX2SetColorControl);
IMPORT(GX2SetBlendControl);
IMPORT(GX2SetBlendConstantColor);
IMPORT(GX2SetCullOnlyControl);
IMPORT(GX2CalcFetchShaderSizeEx);
IMPORT(GX2InitFetchShaderEx);
IMPORT(GX2SetFetchShader);
IMPORT(GX2SetVertexShader);
IMPORT(GX2SetPixelShader);
IMPORT(GX2SetGeometryShader);
IMPORT(GX2SetGeometryUniformBlock);
IMPORT(GX2SetVertexUniformBlock);
IMPORT(GX2SetPixelUniformBlock);
IMPORT(GX2CalcGeometryShaderInputRingBufferSize);
IMPORT(GX2CalcGeometryShaderOutputRingBufferSize);
IMPORT(GX2SetGeometryShaderInputRingBuffer);
IMPORT(GX2SetGeometryShaderOutputRingBuffer);
IMPORT(GX2SetShaderModeEx);
IMPORT(GX2SetAttribBuffer);
IMPORT(GX2InitTextureRegs);
IMPORT(GX2InitSampler);
IMPORT(GX2SetPixelTexture);
IMPORT(GX2SetPixelSampler);
IMPORT(GX2ClearColor);
IMPORT(GX2CopyColorBufferToScanBuffer);
IMPORT(GX2SwapScanBuffers);
IMPORT(GX2Flush);
IMPORT(GX2WaitForVsync);
IMPORT(GX2SetTVEnable);
IMPORT(GX2SetDRCEnable);
IMPORT(GX2SetSwapInterval);
IMPORT(GX2DrawDone);
IMPORT(GX2Shutdown);
IMPORT(GX2DrawEx);
IMPORT(GX2WaitForFlip);
IMPORT(GX2GetSwapStatus);

IMPORT_END();

/* nn_ac */
IMPORT_BEGIN(nn_ac);
IMPORT(ACInitialize);
IMPORT(ACFinalize);
IMPORT(ACConnect);
IMPORT(ACClose);
IMPORT(ACGetAssignedAddress);
IMPORT(ACGetAssignedSubnet);
IMPORT_END();

/* proc_ui */
IMPORT_BEGIN(proc_ui);

IMPORT(ProcUIInit);
IMPORT(ProcUIShutdown);

IMPORT_END();

/* sndcore2 */
IMPORT_BEGIN(sndcore2);

IMPORT(AXInitWithParams);
IMPORT(AXQuit);
IMPORT(AXRegisterFrameCallback);
IMPORT(AXAcquireMultiVoice);
IMPORT(AXSetMultiVoiceDeviceMix);
IMPORT(AXSetMultiVoiceOffsets);
IMPORT(AXSetMultiVoiceCurrentOffset);
IMPORT(AXSetMultiVoiceState);
IMPORT(AXSetMultiVoiceVe);
IMPORT(AXSetMultiVoiceSrcType);
IMPORT(AXSetMultiVoiceSrcRatio);
IMPORT(AXIsMultiVoiceRunning);
IMPORT(AXFreeMultiVoice);

IMPORT_END();

/* sysapp */
IMPORT_BEGIN(sysapp);

IMPORT(SYSRelaunchTitle);
IMPORT(SYSLaunchMenu);

IMPORT_END();

/* vpad */
IMPORT_BEGIN(vpad);

IMPORT(VPADRead);
IMPORT(VPADInit);
IMPORT(VPADGetTPCalibratedPoint);

IMPORT_END();

/* padscore */
IMPORT_BEGIN(padscore);
IMPORT(KPADInit);
IMPORT(WPADProbe);
IMPORT(KPADSetConnectCallback);

IMPORT(WPADSetDataFormat);
IMPORT(WPADEnableURCC);
IMPORT(WPADEnableWiiRemote);
IMPORT(WPADRead);
IMPORT(KPADRead);
IMPORT(KPADReadEx);
IMPORT(KPADShutdown);
IMPORT_END();

/* nsyskbd */
IMPORT_BEGIN(nsyskbd);

IMPORT(KBDSetup);
IMPORT(KBDTeardown);

IMPORT_END();

/* syshid */
#ifdef WIIU_HID
IMPORT_BEGIN(nsyshid);

IMPORT(HIDSetup);
IMPORT(HIDTeardown);
IMPORT(HIDAddClient);
IMPORT(HIDDelClient);
IMPORT(HIDGetDescriptor);
IMPORT(HIDSetDescriptor);
IMPORT(HIDGetReport);
IMPORT(HIDSetReport);
IMPORT(HIDSetIdle);
IMPORT(HIDSetProtocol);
IMPORT(HIDGetProtocol);
IMPORT(HIDRead);
IMPORT(HIDWrite);

IMPORT_END();
#endif
