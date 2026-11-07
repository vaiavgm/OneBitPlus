#pragma once

// Chain presets: the factory recipes plus .obpchain text files in a user folder (read and write) and optional
// shared folders (read only, e.g. next to the installation). A preset is only a starting point: a sample keeps its
// own copy of the chain, so changing or deleting a preset never changes a sample.
//
// File format (plain text):
//   # OneBitPlus chain preset
//   name=Kick 2
//   chain=normalize->saturate(3)->gain(6)->deltasigma

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "src/PipelineModel.h"

namespace ChainPresets
{

namespace fs = std::filesystem;

struct Preset
{
  std::string name;
  PipelineModel::Chain chain;
  bool factory = false;
  bool user = false; // lives in the user folder, so it can be deleted
};

inline const char* kExtension = ".obpchain";

// A file name for a preset name: letters, digits, space, - and _ are kept, everything else becomes _.
inline std::string FileNameFor(const std::string& name)
{
  std::string out;
  for (char c : name)
    out += (std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '-' || c == '_') ? c : '_';
  while (!out.empty() && out.back() == ' ')
    out.pop_back();
  return out + kExtension;
}

inline std::string Serialize(const std::string& name, const PipelineModel::Chain& chain)
{
  return "# OneBitPlus chain preset\nname=" + name + "\nchain=" + PipelineModel::ToText(chain) + "\n";
}

// False if the text has no name or no valid chain.
inline bool Parse(const std::string& text, std::string& name, PipelineModel::Chain& chain)
{
  std::istringstream in(text);
  std::string line, chainText;
  bool haveName = false, haveChain = false;
  while (std::getline(in, line))
  {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty() || line[0] == '#')
      continue;
    const size_t eq = line.find('=');
    if (eq == std::string::npos)
      continue;
    const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
    if (key == "name")
      haveName = !(name = value).empty();
    else if (key == "chain")
      haveChain = PipelineModel::ParseChain(value, chain);
  }
  return haveName && haveChain;
}

class Store
{
public:
  // userDir: where presets are saved (created when needed). sharedDirs: more folders to read presets from.
  explicit Store(fs::path userDir, std::vector<fs::path> sharedDirs = {})
    : mUserDir(std::move(userDir))
    , mSharedDirs(std::move(sharedDirs))
  {
  }

  const fs::path& UserDir() const { return mUserDir; }

  // Factory presets first (in their order), then shared and user folder presets sorted by name.
  std::vector<Preset> List() const
  {
    std::vector<Preset> out;
    for (const auto& f : PipelineModel::FactoryChains())
    {
      Preset p;
      p.name = f.name;
      p.factory = true;
      if (PipelineModel::ParseChain(f.text, p.chain))
        out.push_back(std::move(p));
    }

    std::vector<Preset> files;
    for (const auto& dir : mSharedDirs)
      ReadDir(dir, false, files);
    ReadDir(mUserDir, true, files);
    std::stable_sort(files.begin(), files.end(), [](const Preset& a, const Preset& b) { return a.name < b.name; });
    out.insert(out.end(), files.begin(), files.end());
    return out;
  }

  // Saves (or overwrites) a preset in the user folder.
  bool Save(const std::string& name, const PipelineModel::Chain& chain, std::string* error = nullptr) const
  {
    if (name.empty() || FileNameFor(name) == kExtension)
      return Fail(error, "the preset needs a name");
    std::error_code ec;
    fs::create_directories(mUserDir, ec);
    if (ec)
      return Fail(error, "can't create " + mUserDir.string() + ": " + ec.message());
    std::ofstream file(mUserDir / FileNameFor(name), std::ios::binary | std::ios::trunc);
    if (!file)
      return Fail(error, "can't write " + (mUserDir / FileNameFor(name)).string());
    file << Serialize(name, chain);
    return static_cast<bool>(file) || Fail(error, "writing failed");
  }

  // Deletes a preset from the user folder (factory and shared presets can't be deleted).
  bool Delete(const std::string& name) const
  {
    std::error_code ec;
    return fs::remove(mUserDir / FileNameFor(name), ec) && !ec;
  }

private:
  static bool Fail(std::string* error, const std::string& why)
  {
    if (error)
      *error = why;
    return false;
  }

  static void ReadDir(const fs::path& dir, bool user, std::vector<Preset>& out)
  {
    std::error_code ec;
    if (!fs::is_directory(dir, ec))
      return;
    for (const auto& entry : fs::directory_iterator(dir, ec))
    {
      if (!entry.is_regular_file(ec) || entry.path().extension() != kExtension)
        continue;
      std::ifstream file(entry.path(), std::ios::binary);
      std::stringstream text;
      text << file.rdbuf();
      Preset p;
      if (Parse(text.str(), p.name, p.chain))
      {
        p.user = user;
        out.push_back(std::move(p));
      }
    }
  }

  fs::path mUserDir;
  std::vector<fs::path> mSharedDirs;
};

} // namespace ChainPresets
