/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UTIL_HPP
#define HYPERION_UTIL_HPP

#include <core/containers/StaticString.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {

// tuple forward declaration
namespace utilities {

template <class... Types>
class Tuple;

} // namespace utilities

using utilities::Tuple;

namespace detail {

template <auto Str, bool ShouldStripNamespace>
constexpr auto ParseTypeName();

// strip "class " or "struct " from beginning StaticString
template <auto Str>
constexpr auto StripClassOrStruct()
{
    constexpr auto class_index = Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString("class ") > >();
    constexpr auto struct_index = Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString("struct ") > >();

    if constexpr (class_index != -1 && (struct_index == -1 || class_index <= struct_index)) {
        return containers::helpers::Substr< Str, class_index + 6, Str.Size() >::value; // 6 = length of "class "
    } else if constexpr (struct_index != -1 && (class_index == -1 || struct_index <= class_index)) {
        return containers::helpers::Substr< Str, struct_index + 7, Str.Size() >::value; // 7 = length of "struct "
    } else {
        return Str;
    }
}

// constexpr functions to strip namespaces from StaticString

template <bool ShouldStripNamespace>
struct TypeNameStringTransformer
{
    static constexpr char delimiter = ',';

    static constexpr uint32 balance_bracket_options = containers::helpers::BALANCE_BRACKETS_ANGLE;

    template <auto String>
    static constexpr auto Transform()
    {
        constexpr SizeType last_index = ShouldStripNamespace
            ? containers::helpers::Trim< String >::value.template FindLast< containers::detail::IntegerSequenceFromString< StaticString("::") > >()
            : SizeType(-1);

        if constexpr (last_index == -1) {
            return StripClassOrStruct< containers::helpers::Trim< String >::value >();
        } else {
            return StripClassOrStruct< containers::helpers::Substr< containers::helpers::Trim< String >::value, last_index + 2, SizeType(-1) >::value >();
        }
    }
};

template <bool ShouldStripNamespace>
struct TypeNameStringTransformer2
{
    static constexpr char delimiter = ',';

    static constexpr uint32 balance_bracket_options = containers::helpers::BALANCE_BRACKETS_ANGLE;

    template <auto String>
    static constexpr auto Transform()
    {
        return ParseTypeName< String, ShouldStripNamespace >();
    }
};

template <auto Str, bool ShouldStripNamespace>
constexpr auto ParseTypeName()
{
    constexpr auto left_arrow_index = Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString("<") > >();
    constexpr auto right_arrow_index = Str.template FindLast< containers::detail::IntegerSequenceFromString< StaticString(">") > >();


    if constexpr (left_arrow_index != SizeType(-1) && right_arrow_index != SizeType(-1)) {
        static_assert(left_arrow_index < right_arrow_index, "Left arrow index must be less than right arrow index or parsing will fail!");

        return containers::helpers::Concat<
            containers::helpers::TransformSplit< TypeNameStringTransformer2<ShouldStripNamespace>, containers::helpers::Substr< Str, 0, left_arrow_index >::value >::value,
            StaticString("<"),
            containers::helpers::TransformSplit< TypeNameStringTransformer2<ShouldStripNamespace>, containers::helpers::Substr< Str, left_arrow_index + 1, right_arrow_index >::value >::value,
            StaticString(">")
        >::value;
    } else {
        return containers::helpers::TransformSplit< TypeNameStringTransformer< ShouldStripNamespace >, Str >::value;
    }
}

} // namespace detail

/*! \brief Returns the name of the type T as a StaticString.
 *
 *  \tparam T The type to get the name of.
 *
 *  \return The name of the type T as a StaticString.
 */
template <class T>
constexpr auto TypeName()
{
    constexpr StaticString<sizeof(HYP_FUNCTION_NAME_LIT)> name(HYP_FUNCTION_NAME_LIT);

#ifdef HYP_CLANG_OR_GCC
#ifdef HYP_CLANG
    // auto hyperion::TypeName() [T = hyperion::Task<int, int>]
    constexpr auto substr = containers::helpers::Substr< name, 31, sizeof(HYP_FUNCTION_NAME_LIT) - 2 >::value;
#elif defined(HYP_GCC)
    // constexpr auto hyperion::TypeName() [with T = hyperion::Task<int, int>]
    constexpr auto substr = containers::helpers::Substr< name, 47, sizeof(HYP_FUNCTION_NAME_LIT) - 2 >::value;
#endif
#elif defined(HYP_MSVC)
    //  auto __cdecl hyperion::TypeName<class hyperion::Task<int,int>>(void)
    constexpr auto substr = containers::helpers::Substr< name, 32, sizeof(HYP_FUNCTION_NAME_LIT) - 8 >::value;

#else
    static_assert(false, "Unsupported compiler for TypeName()");
#endif

    return detail::ParseTypeName< substr, false >();
}

/*! \brief Returns the name of the type T as a StaticString. Removes the namespace from the name (e.g. hyperion::Task<int, int> -> Task<int, int>).
 *
 *  \tparam T The type to get the name of.
 *
 *  \return The name of the type T as a StaticString.
 */
template <class T>
constexpr auto TypeNameWithoutNamespace()
{
    constexpr StaticString<sizeof(HYP_FUNCTION_NAME_LIT)> name(HYP_FUNCTION_NAME_LIT);
#ifdef HYP_CLANG_OR_GCC
#ifdef HYP_CLANG

    // auto hyperion::TypeNameWithoutNamespace() [T = hyperion::Task<int, int>]
    constexpr auto substr = containers::helpers::Substr< name, 47, sizeof(HYP_FUNCTION_NAME_LIT) - 2 >::value;
#elif defined(HYP_GCC)
    // constexpr auto hyperion::TypeNameWithoutNamespace() [with T = hyperion::Task<int, int>]
    constexpr auto substr = containers::helpers::Substr< name, 63, sizeof(HYP_FUNCTION_NAME_LIT) - 2 >::value;
#endif
#elif defined(HYP_MSVC)
    //  auto __cdecl hyperion::TypeNameWithoutNamespace<class hyperion::Task<int,int>>(void)
    constexpr auto substr = containers::helpers::Substr< name, 48, sizeof(HYP_FUNCTION_NAME_LIT) - 8 >::value;
#else
    static_assert(false, "Unsupported compiler for TypeNameWithoutNamespace()");
#endif
    
    return detail::ParseTypeName< substr, true >();
}

template <class T, bool ShouldStripNamespace = false>
struct TypeNameHelper;

template <class T>
struct TypeNameHelper<T, false>
{
    static constexpr decltype(TypeName<T>()) value = TypeName<T>();
};

template <class T>
struct TypeNameHelper<T, true>
{
    static constexpr decltype(TypeNameWithoutNamespace<T>()) value = TypeNameWithoutNamespace<T>();
};

namespace detail {

template <auto Str>
constexpr auto StripReturnType()
{
    constexpr auto first_space_index = Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString(" ") > >();

    if constexpr (first_space_index == SizeType(-1)) {
        return Str;
    } else {
        constexpr auto without_return_type = containers::helpers::Substr< Str, first_space_index + 1, Str.Size() >::value;

        constexpr auto left_angle_bracket_index = without_return_type.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString("<") > >();
        constexpr auto left_parenthesis_index = without_return_type.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString("(") > >();

        constexpr auto first_token_index = left_angle_bracket_index != SizeType(-1) && (left_parenthesis_index == SizeType(-1) || left_angle_bracket_index < left_parenthesis_index)
            ? left_angle_bracket_index
            : left_parenthesis_index;

        constexpr auto second_space_index = without_return_type.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString(" ") > >();

        if constexpr (second_space_index != SizeType(-1) && first_token_index != SizeType(-1) && second_space_index < first_token_index) {
            return containers::helpers::Substr< without_return_type, second_space_index + 1, without_return_type.Size() >::value;
        } else {
            return without_return_type;
        }
    }
}

template <auto Str>
constexpr auto StripNamespaceFromFunctionName()
{
    if constexpr (Str.Size() == 0) {
        return Str;
    } else if constexpr (Str.data[0] >= 'A' && Str.data[0] <= 'Z') {
        // function and class names start with uppercase letters in hyperion
        return Str;
    } else {
        constexpr auto first_colon_index = Str.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString("::") > >();

        if constexpr (first_colon_index != SizeType(-1)) {
            constexpr auto substr = containers::helpers::Substr< Str, first_colon_index + 2, Str.Size() >::value;

            return StripNamespaceFromFunctionName< substr >();
        } else {
            return Str;
        }
    }
}

} // namespace detail

/*! \brief Normalizes the input string, removing the return type and parameters from the function signature. */
template <auto Str>
constexpr auto PrettyFunctionName()
{
    constexpr auto without_return_type = detail::StripReturnType< Str >();

    constexpr auto left_angle_bracket_index = without_return_type.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString("<") > >();
    constexpr auto left_parenthesis_index = without_return_type.template FindFirst< containers::detail::IntegerSequenceFromString< StaticString("(") > >();

    if constexpr (left_parenthesis_index != SizeType(-1)) {
        if constexpr (left_angle_bracket_index != SizeType(-1) && left_angle_bracket_index < left_parenthesis_index) {
            return detail::StripNamespaceFromFunctionName< containers::helpers::Substr< without_return_type, 0, left_angle_bracket_index >::value >();
        } else {
            return detail::StripNamespaceFromFunctionName< containers::helpers::Substr< without_return_type, 0, left_parenthesis_index >::value >();
        }
    } else {
        return without_return_type;
    }
}

#define HYP_PRETTY_FUNCTION_NAME hyperion::PrettyFunctionName< HYP_STATIC_STRING(HYP_FUNCTION_NAME_LIT) >()

template <class T>
struct TypeWrapper
{
    using Type = T;
};

/*! \brief Size of an array literal (Hyperion equivalent of std::size) */
template <class T, uint N>
constexpr uint ArraySize(const T (&)[N])
{
    return N;
}

/*! \brief Convert the value to an rvalue reference. If it cannot be converted, a compile time error will be generated.
 *  \tparam T The type of the value being passed
 *  \param value The value to convert to an rvalue reference.
 *  \returns The value as an rvalue reference. */
template <class T>
HYP_FORCE_INLINE constexpr std::remove_reference_t<T> &&Move(T &&value) noexcept
{
    static_assert(std::is_lvalue_reference_v<T>, "T must be an lvalue reference to use Move()");
    static_assert(!std::is_same_v<const typename std::remove_reference_t<T> &, typename std::remove_reference_t<T> &> , "T must not be const to use Move()");

    return static_cast<std::remove_reference_t<T> &&>(value);
}

/*! \brief Attempts to move the object if possible, will use copy otherwise.
 *  \tparam T The type of the value being passed
 *  \param value The value to convert to an rvalue reference.
 *  \returns The value as an rvalue reference. */
template <class T>
HYP_FORCE_INLINE constexpr std::remove_reference_t<T> &&TryMove(T &&value) noexcept
{
    return static_cast<std::remove_reference_t<T> &&>(value);
}

template <class T>
HYP_FORCE_INLINE void Swap(T &a, T &b)
{
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>, "Swap requires T to be move constructible and move assignable");

    T temp = TryMove(a);
    a = TryMove(b);
    b = TryMove(temp);
}

#pragma region Algorithms

template <class IteratorType, class ValueType>
HYP_FORCE_INLINE IteratorType Find(IteratorType _begin, IteratorType _end, ValueType &&value)
{
    for (auto it = _begin; it != _end; ++it) {
        if (*it == value) {
            return it;
        }
    }

    return _end;
}

template <class IteratorType, class PredicateType>
HYP_FORCE_INLINE IteratorType FindIf(IteratorType _begin, IteratorType _end, PredicateType &&predicate)
{
    for (auto it = _begin; it != _end; ++it) {
        if (predicate(*it)) {
            return it;
        }
    }

    return _end;
}

#pragma endregion Algorithms

#pragma region StaticMessage

template <auto StaticString>
struct StaticMessage
{
    static constexpr auto value = StaticString;
};

template <auto StaticString>
constexpr HYP_FORCE_INLINE StaticMessage<StaticString> MakeStaticMessage()
{
    return StaticMessage<StaticString> { };
}

#define HYP_STATIC_MESSAGE(str) MakeStaticMessage<HYP_STATIC_STRING(str)>()

#pragma endregion StaticMessage

} // namespace hyperion

#endif
