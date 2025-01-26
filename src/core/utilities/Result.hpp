/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RESULT_HPP
#define HYPERION_RESULT_HPP

#include <core/utilities/Variant.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/StringView.hpp>

#include <core/utilities/StaticMessage.hpp>

#include <core/system/Debug.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion {
namespace utilities {

class IError
{
public:
    virtual ~IError() = default;

    virtual operator bool() const = 0;

    virtual UTF8StringView GetMessage() const = 0;
    virtual ANSIStringView GetFunctionName() const = 0;
};

class Error : public IError
{
protected:
    Error()
        : m_static_message {"", "<unknown>" }
    {
    }

    Error(UTF8StringView message)
        : m_static_message { message, "<unknown>" }
    {
    }

public:
    Error(const StaticMessage &static_message)
        : m_static_message { static_message }
    {
    }

    virtual ~Error() override = default;

    virtual operator bool() const override
    {
        return true;
    }

    virtual UTF8StringView GetMessage() const override
    {
        return m_static_message.message;
    }

    virtual ANSIStringView GetFunctionName() const override
    {
        return m_static_message.current_function;
    }

protected:
    StaticMessage   m_static_message;
};

template <class ErrorType>
class NullError final : public ErrorType
{
public:
    static_assert(std::is_base_of_v<IError, ErrorType>, "ErrorType must implement IError");

    static const NullError<ErrorType> &GetInstance()
    {
        static const NullError<ErrorType> instance;

        return instance;
    }

    virtual ~NullError() override = default;

    virtual operator bool() const override
    {
        return false;
    }
};

#define HYP_MAKE_ERROR(ErrorType, message, ...) ErrorType(HYP_STATIC_MESSAGE(message), ##__VA_ARGS__)

/*! \brief A class that represents a result that can either be a value or an error.
 *  The value and error types are specified by the template parameters.
 *  The error type defaults to Error if not specified. */
template <class T = void, class ErrorType = Error>
class Result;

template <class T, class ErrorType>
class Result
{
public:
    static_assert(std::is_base_of_v<IError, ErrorType>, "ErrorType must implement IError");

    Result(const T &value)
        : m_value(value)
    {
    }

    Result(T &&value)
        : m_value(std::move(value))
    {
    }

    Result(const ErrorType &error)
        : m_value(error)
    {
    }

    Result(ErrorType &&error)
        : m_value(std::move(error))
    {
    }

    Result(const Result &other)                 = default;
    Result &operator=(const Result &other)      = default;
    Result(Result &&other) noexcept             = default;
    Result &operator=(Result &&other) noexcept  = default;
    ~Result()                                   = default;

    HYP_FORCE_INLINE explicit operator bool() const
        { return m_value.template Is<T>(); }

    HYP_FORCE_INLINE bool operator!() const
        { return !m_value.template Is<T>(); }

    HYP_FORCE_INLINE bool HasValue() const
        { return m_value.template Is<T>(); }

    HYP_FORCE_INLINE bool HasError() const
        { return m_value.template Is<ErrorType>(); }

    HYP_FORCE_INLINE T &GetValue() &
    {
        AssertThrowMsg(HasValue(), "Result does not contain a value");

        return m_value.template GetUnchecked<T>();
    }

    HYP_FORCE_INLINE const T &GetValue() const &
    {
        AssertThrowMsg(HasValue(), "Result does not contain a value");

        return m_value.template GetUnchecked<T>();
    }

    HYP_FORCE_INLINE T GetValue() &&
    {
        AssertThrowMsg(HasValue(), "Result does not contain a value");

        return std::move(m_value.template GetUnchecked<T>());
    }

    HYP_FORCE_INLINE T GetValueOr(T &&default_value) const &
    {
        if (HasValue()) {
            return m_value.template GetUnchecked<T>();
        }

        return std::forward<T>(default_value);
    }

    HYP_FORCE_INLINE T GetValueOr(T &&default_value) &&
    {
        if (HasValue()) {
            return std::move(m_value.template GetUnchecked<T>());
        } else {
            return std::forward<T>(default_value);
        }
    }

    HYP_FORCE_INLINE const ErrorType &GetError() const
    {
        if (HasError()) {
            return m_value.template GetUnchecked<ErrorType>();
        } else {
            return NullError<ErrorType>::GetInstance();
        }
    }

    HYP_FORCE_INLINE T &operator*()
        { return GetValue(); }

    HYP_FORCE_INLINE const T &operator*() const
        { return GetValue(); }

    HYP_FORCE_INLINE T *operator->()
        { return &GetValue(); }

    HYP_FORCE_INLINE const T *operator->() const
        { return &GetValue(); }

    HYP_FORCE_INLINE bool operator==(const Result &other) const
    {
        if (HasValue() != other.HasValue()) {
            return false;
        }

        if (HasValue()) {
            return GetValue() == other.GetValue();
        }

        return true;
    }

    HYP_FORCE_INLINE bool operator!=(const Result &other) const
    {
        if (HasValue() != other.HasValue()) {
            return true;
        }

        if (HasValue()) {
            return GetValue() != other.GetValue();
        }

        return false;
    }

    HYP_FORCE_INLINE bool operator==(const T &value) const
        { return HasValue() && m_value.template GetUnchecked<T>() == value; }

    HYP_FORCE_INLINE bool operator!=(const T &value) const
        { return !HasValue() || m_value.template GetUnchecked<T>() == value; }

    HYP_FORCE_INLINE bool operator==(const ErrorType &error) const = delete;
    HYP_FORCE_INLINE bool operator!=(const ErrorType &error) const = delete;

private:
    Variant<T, ErrorType>   m_value;
};

template <class ErrorType>
class Result<void, ErrorType>
{
public:
    static_assert(std::is_base_of_v<IError, ErrorType>, "ErrorType must implement IError");

    Result()                                    = default;

    Result(const ErrorType &error)
        : m_error(error)
    {
    }

    Result(ErrorType &&error)
        : m_error(std::move(error))
    {
    }

    Result(const Result &other)                 = default;
    Result &operator=(const Result &other)      = default;
    Result(Result &&other) noexcept             = default;
    Result &operator=(Result &&other) noexcept  = default;
    ~Result()                                   = default;

    HYP_FORCE_INLINE explicit operator bool() const
        { return !m_error.HasValue(); }

    HYP_FORCE_INLINE bool operator!() const
        { return m_error.HasValue(); }

    HYP_FORCE_INLINE bool HasValue() const
        { return !m_error.HasValue(); }

    HYP_FORCE_INLINE bool HasError() const
        { return m_error.HasValue(); }

    HYP_FORCE_INLINE const ErrorType &GetError() const
    {
        if (HasError()) {
            return m_error.Get();
        } else {
            return NullError<ErrorType>::GetInstance();
        }
    }

    HYP_FORCE_INLINE bool operator==(const Result &other) const
        { return m_error.HasValue() == other.m_error.HasValue(); }

    HYP_FORCE_INLINE bool operator!=(const Result &other) const
        { return m_error.HasValue() != other.m_error.HasValue(); }

    HYP_FORCE_INLINE bool operator==(const ErrorType &error) const = delete;
    HYP_FORCE_INLINE bool operator!=(const ErrorType &error) const = delete;

private:
    Optional<ErrorType> m_error;
};

} // namespace utilities

using utilities::Result;
using utilities::IError;
using utilities::Error;

} // namespace hyperion

#endif