#pragma once

#include <string>

class Version {
  public:
    Version(HMODULE hModule);
    ~Version();

    std::string productVersionString();

  private:
    LPVOID lpResCopy;
};