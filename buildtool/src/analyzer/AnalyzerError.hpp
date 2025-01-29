/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_ANALYZER_ERROR_HPP
#define HYPERION_BUILDTOOL_ANALYZER_ERROR_HPP

#include <core/utilities/Result.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace buildtool {

class AnalyzerError : public Error
{
public:
    AnalyzerError()
        : Error(),
          m_error_code(0)
    {
    }

    AnalyzerError(const StaticMessage &static_message, const FilePath &path, int error_code = 0)
        : Error(static_message),
          m_path(path),
          m_error_code(error_code)
    {
    }

    virtual ~AnalyzerError() override = default;

    virtual operator bool() const override
    {
        return true;
    }

    HYP_FORCE_INLINE const FilePath &GetPath() const
        { return m_path; }

    HYP_FORCE_INLINE int GetErrorCode() const
        { return m_error_code; }

private:
    FilePath    m_path;
    int         m_error_code;
};

} // namespace buildtool
} // namespace hyperion

#endif
