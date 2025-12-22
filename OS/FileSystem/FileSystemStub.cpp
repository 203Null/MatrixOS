#include "FileSystem.h"

namespace MatrixOS::FileSystem
{
  void Init()
  {
  }

  bool Available(void)
  {
    return false;
  }

  bool Exists(const string& path)
  {
    (void)path;
    return false;
  }

  bool MakeDir(const string& path)
  {
    (void)path;
    return false;
  }

  File Open(const string& path, const string& mode)
  {
    File file;
    (void)file._Open(path, mode);
    return file;
  }

  bool Remove(const string& path)
  {
    (void)path;
    return false;
  }

  bool RemoveDir(const string& path)
  {
    (void)path;
    return false;
  }

  bool Rename(const string& from, const string& to)
  {
    (void)from;
    (void)to;
    return false;
  }

  vector<string> ListDir(const string& path)
  {
    (void)path;
    return {};
  }
}
