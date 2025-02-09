#ifndef HYPERION_V2_FS_UTIL_H
#define HYPERION_V2_FS_UTIL_H

#include <core/Containers.hpp>
#include <util/Defines.hpp>
#include <util/StringUtil.hpp>
#include <asset/BufferedByteReader.hpp>

#include <string>
#include <cstring>
#include <array>

namespace hyperion::v2 {

class FileSystem
{
public:
    static bool DirExists(const std::string &path);
    static int Mkdir(const std::string &path);
    static std::string CurrentPath();
    static std::string RelativePath(const std::string &path, const std::string &base);

    template <class ...String>
    static inline std::string Join(String &&... args)
    {
        std::array<std::string, sizeof...(args)> args_array = {args...};

        enum {
            SEPARATOR_MODE_WINDOWS,
            SEPARATOR_MODE_UNIX
        } separator_mode;

        if (!std::strcmp(HYP_FILESYSTEM_SEPARATOR, "\\")) {
            separator_mode = SEPARATOR_MODE_WINDOWS;
        } else {
            separator_mode = SEPARATOR_MODE_UNIX;
        }

        for (auto &arg : args_array) {
            if (separator_mode == SEPARATOR_MODE_WINDOWS) {
                arg = StringUtil::ReplaceAll(arg, "/", "\\");
            } else {
                arg = StringUtil::ReplaceAll(arg, "\\", "/");
            }
        }

        return StringUtil::Join(args_array, HYP_FILESYSTEM_SEPARATOR);
    }
};

class FilePath : public String
{
protected:
    using Base = String;

public:
    FilePath() : String() { }
    FilePath(const String::ValueType *str) : String(str) { }
    FilePath(const FilePath &other) : String(other) { }
    FilePath(const String &str) : String(str) { }
    FilePath(FilePath &&other) noexcept : String(std::move(other)) { }
    FilePath(String &str) noexcept : String(std::move(str)) { }
    ~FilePath() = default;

    FilePath &operator=(const FilePath &other)
    {
        String::operator=(other);

        return *this;
    }

    FilePath &operator=(FilePath &&other) noexcept
    {
        String::operator=(std::move(other));

        return *this;
    }

    FilePath operator+(const FilePath &other)
    {
        return Join(Data(), other.Data());
    }

    FilePath &operator+=(const FilePath &other)
    {
        return *this = Join(Data(), other.Data());
    }

    bool Exists() const;
    bool IsDirectory() const;

    BufferedReader<2048> Open() const;

    static inline FilePath Current()
    {
        return FilePath(FileSystem::CurrentPath().c_str());
    }

    static inline FilePath Relative(const FilePath &path, const FilePath &base)
    {
        return FilePath(FileSystem::RelativePath(path.Data(), base.Data()).c_str());
    }

    template <class ... Paths>
    static inline FilePath Join(Paths &&... paths)
    {
        const auto str = FileSystem::Join(std::forward<Paths>(paths)...);

        return FilePath(str.c_str());
    }
};

} // namespace hyperion::v2

#endif