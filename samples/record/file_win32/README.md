# Native overlapped recording output

`make check` builds the non-Windows stub test on Linux. With mingw-w64,
`make check CC=x86_64-w64-mingw32-gcc EXE=.exe RUNNER=wine` tests the Windows
implementation. Native Windows uses `make check EXE=.exe`.

The fault-injection test covers delayed and immediate completion, one bounded
check per step, BUSY ownership, short writes, continuation failure, zero/invalid
completion sizes, offsets across 4 GiB, signed file-offset overflow, disk-full
errors and flush/close failures. The real test compares all four MiB after
successful finalization and verifies that existing files are not overwritten.
The MP4 mux fixture uses this writer on Windows, retaining each mux borrow until
the write completes; its independent decoder checks video, AC-3 and timing.

Production callers must use an I/O worker. No scheduler or thread is installed
by this component; Windows can block even an overlapped WriteFile, and flush
and close belong off all render/audio/main-thread paths. The shared VFS does
not expose an asynchronous-write contract, so arbitrary VFS streams are not
accepted here. Finish refuses in-flight writes, flushes completed output, then
closes; no power-loss guarantee is claimed. Cancellation requests use CancelIoEx without waiting; retain both the object
and borrowed buffer until step reports terminal completion. A raced successful
completion is still treated as cancellation, without issuing a short-write
continuation.

The worker can borrow `record_file_win32_wait_handle()` while a write or
cancellation is pending. This is the file handle itself: the writer permits
only one outstanding I/O, so completion is unambiguous. Do not close/reset it;
service completion and re-query before waiting again. No wait is added to
the writer. The real disk test waits on this handle before checking bytes.
