#pragma once

#include "Framework.h"

// Seek mode enum
enum SeekMode : int {
  FROM_START = 0,   // From beginning of file
  FROM_CURRENT = 1, // From current position
  FROM_END = 2      // From end of file
};

#if DEVICE_STORAGE == 1
#include "FatFS/ff.h"

// File class wrapper around FatFS FIL
class File
{
private:
  FIL file_handle;
  string file_path;
  bool is_open;

public:
  File();
  ~File();

  // File operations
  string Name();
  bool Available();
  bool Close();
  bool Flush();
  int Peek();
  size_t Position();
  void Print(const string& data);
  void Println(const string& data);
  bool Seek(size_t position, SeekMode whence = FROM_START);
  size_t Size();
  size_t Read(void* buffer, size_t length);
  size_t Write(const void* buffer, size_t length);
  bool IsDirectory();

  // Internal methods (for FileSystem use)
  bool _Open(const string& path, const string& mode);
  FIL* _GetHandle() { return &file_handle; }
};

#else

class File
{
public:
  File() {}
  ~File() {}

  string Name() { return string(); }
  bool Available() { return false; }
  bool Close() { return false; }
  bool Flush() { return false; }
  int Peek() { return -1; }
  size_t Position() { return 0; }
  void Print(const string& data) { (void)data; }
  void Println(const string& data) { (void)data; }
  bool Seek(size_t position, SeekMode whence = FROM_START) { (void)position; (void)whence; return false; }
  size_t Size() { return 0; }
  size_t Read(void* buffer, size_t length) { (void)buffer; (void)length; return 0; }
  size_t Write(const void* buffer, size_t length) { (void)buffer; (void)length; return 0; }
  bool IsDirectory() { return false; }

  bool _Open(const string& path, const string& mode) { (void)path; (void)mode; return false; }
  void* _GetHandle() { return nullptr; }
};

#endif
