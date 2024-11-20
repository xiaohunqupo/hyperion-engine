/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TYPE_ID_HPP
#define HYPERION_TYPE_ID_HPP

#include <core/threading/Mutex.hpp>
#include <core/Name.hpp>
#include <core/Util.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

#include <atomic>

namespace hyperion {
namespace utilities {

using TypeIDValue = uint32;

static constexpr uint32 g_type_id_hash_bit_offset = 2;
static constexpr uint32 g_type_id_hash_max = (~0u << g_type_id_hash_bit_offset) >> g_type_id_hash_bit_offset;
static constexpr uint32 g_type_id_flag_bit_mask = 0x3u;
static constexpr uint32 g_type_id_flag_max = 0x3u;

enum TypeIDFlags : uint8
{
    TYPE_ID_FLAGS_NONE          = 0x0,
    TYPE_ID_FLAGS_DYNAMIC       = 0x1, // Type is dynamic - does not map 1:1 to a native C++ type. E.g C# class
    TYPE_ID_FLAGS_PLACEHOLDER   = 0x2
};

namespace detail {

template <class T, uint8 Flags>
struct TypeID_Impl
{
    static_assert(Flags <= g_type_id_flag_max, "Flags cannot be > 0x3");

    static constexpr TypeIDValue value = ((TypeNameWithoutNamespace<T>().GetHashCode().Value() % HashCode::ValueType(g_type_id_hash_max)) << g_type_id_hash_bit_offset) | (Flags & g_type_id_flag_max);
};

template <uint8 Flags>
struct TypeID_Impl<void, Flags>
{
    static constexpr TypeIDValue value = 0;
};

template <uint8 Flags>
struct TypeID_FromString_Impl
{
    static_assert(Flags <= g_type_id_flag_max, "Flags cannot be > 0x3");

    constexpr TypeIDValue operator()(const char *str) const
    {
        return ((HashCode::GetHashCode(str).Value() % HashCode::ValueType(g_type_id_hash_max)) << g_type_id_hash_bit_offset) | (Flags & g_type_id_flag_max);
    }
};

} // namespace detail

/*! \brief Simple 32-bit identifier for a given type. Stable across DLLs as the type hash is based on the name of the type. */
struct TypeID
{
    using ValueType = TypeIDValue;

private:
    ValueType   value;

    static constexpr ValueType void_value = ValueType(0);

public:
    template <class T>
    static constexpr TypeID ForType()
        { return TypeID { detail::TypeID_Impl<T, TypeIDFlags::TYPE_ID_FLAGS_NONE>::value }; }

    static constexpr TypeID ForManagedType(const char *str)
        { return TypeID { detail::TypeID_FromString_Impl<TypeIDFlags::TYPE_ID_FLAGS_DYNAMIC>{}(str) }; }

    constexpr TypeID()
        : value { void_value }
    {
    }

    constexpr TypeID(ValueType id)
        : value(id)
    {
    }

    constexpr TypeID(const TypeID &other)   = default;
    TypeID &operator=(const TypeID &other)  = default;

    constexpr TypeID(TypeID &&other) noexcept
        : value(other.value)
    {
        other.value = void_value;
    }
    
    constexpr TypeID &operator=(TypeID &&other) noexcept
    {
        value = other.value;
        other.value = void_value;

        return *this;
    }

    constexpr TypeID &operator=(ValueType id)
    {
        value = id;

        return *this;
    }
    
    HYP_FORCE_INLINE constexpr explicit operator bool() const
        { return value != void_value; }
    
    HYP_FORCE_INLINE constexpr bool operator!() const
        { return value == void_value; }
    
    HYP_FORCE_INLINE constexpr bool operator==(const TypeID &other) const
        { return value == other.value; }
    
    HYP_FORCE_INLINE constexpr bool operator!=(const TypeID &other) const
        { return value != other.value; }
    
    HYP_FORCE_INLINE constexpr bool operator<(const TypeID &other) const
        { return value < other.value; }
    
    HYP_FORCE_INLINE constexpr bool operator<=(const TypeID &other) const
        { return value <= other.value; }
    
    HYP_FORCE_INLINE constexpr bool operator>(const TypeID &other) const
        { return value > other.value; }
    
    HYP_FORCE_INLINE constexpr bool operator>=(const TypeID &other) const
        { return value >= other.value; }

    HYP_FORCE_INLINE constexpr bool IsNativeType() const
        { return !((value & g_type_id_flag_max) & TYPE_ID_FLAGS_DYNAMIC); }

    HYP_FORCE_INLINE constexpr bool IsDynamicType() const
        { return (value & g_type_id_flag_max) & TYPE_ID_FLAGS_DYNAMIC; }
    
    HYP_FORCE_INLINE constexpr ValueType Value() const
        { return value; }
    
    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
        { return HashCode::GetHashCode(value); }
    
    HYP_FORCE_INLINE static constexpr TypeID Void()
        { return TypeID { void_value }; }
};

} // namespace utilities

using utilities::TypeID;

} // namespace hyperion

#endif
