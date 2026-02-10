// Export FS.ErrnoError, though it can still be renamed internally.
// This is used a lot internally so we save a bit of code size by doing this instead of declaring an extern.
FS["ErrnoError"] = FS.ErrnoError;
