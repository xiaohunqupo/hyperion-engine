/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_DATA_HPP
#define HYPERION_CORE_HYP_DATA_HPP

#include <core/Defines.hpp>

#include <core/ID.hpp>
#include <core/Handle.hpp>

#include <core/object/HypObject.hpp>

#include <core/utilities/TypeID.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/Optional.hpp>

#include <core/memory/Any.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion {

struct HypData;

namespace detail {

template <class T, class T2 = void>
struct HypDataInitializer;

template <class T, class ConvertibleTypesTuple>
struct HypDataTypeChecker_Tuple;

template <class ReturnType, class T, class ConvertibleTypesTuple>
struct HypDataGetter_Tuple;

} // namespace detail

struct HypData
{
    using VariantType = Variant<
        int8,
        int16,
        int32,
        int64,
        uint8,
        uint16,
        uint32,
        uint64,
        float,
        double,
        bool,
        IDBase,
        AnyHandle,
        RC<void>,
        AnyRef,
        Any
    >;

    template <class T>
    static constexpr bool can_store_directly =
        /* Fundamental types - can be stored inline */
        std::is_same_v<T, int8> || std::is_same_v<T, int16> | std::is_same_v<T, int32> | std::is_same_v<T, int64>
        || std::is_same_v<T, uint8> || std::is_same_v<T, uint16> || std::is_same_v<T, uint32> || std::is_same_v<T, uint64>
        || std::is_same_v<T, float> || std::is_same_v<T, double>
        || std::is_same_v<T, bool>
        
        /*! All ID<T> are stored as IDBase */
        || std::is_base_of_v<IDBase, T>

        /*! Handle<T> gets stored as AnyHandle, which holds TypeID for conversion */
        || std::is_base_of_v<HandleBase, T> || std::is_same_v<T, AnyHandle>

        /*! RC<T> gets stored as RC<void> and can be converted back */
        || std::is_base_of_v<typename RC<void>::RefCountedPtrBase, T>

        /*! Pointers are stored as AnyRef which holds TypeID for conversion */
        || std::is_same_v<T, AnyRef> || std::is_pointer_v<T>;

    VariantType value;

    HypData()                                       = default;

    template <class T, typename = std::enable_if_t< !std::is_same_v< T, HypData > > >
    HypData(T &&value)
    {
        detail::HypDataInitializer<NormalizedType<T>>{}.Set(*this, std::forward<T>(value));
    }

    HypData(const HypData &other)                   = delete;
    HypData &operator=(const HypData &other)        = delete;

    HypData(HypData &&other) noexcept               = default;
    HypData &operator=(HypData &&other) noexcept    = default;

    ~HypData()                                      = default;

    HYP_FORCE_INLINE TypeID GetTypeID() const
    {
        if (const Any *any_ptr = value.TryGet<Any>()) {
            return any_ptr->GetTypeID();
        }

        return value.GetTypeID();
    }

    HYP_FORCE_INLINE bool IsValid() const
        { return value.IsValid(); }

    HYP_FORCE_INLINE AnyRef ToRef()
    {
        if (!value.IsValid()) {
            return AnyRef();
        }

        if (AnyRef *any_ref_ptr = value.TryGet<AnyRef>()) {
            return *any_ref_ptr;
        }

        if (Any *any_ptr = value.TryGet<Any>()) {
            return any_ptr->ToRef();
        }

        return AnyRef(value.GetTypeID(), value.GetPointer());
    }

    HYP_FORCE_INLINE ConstAnyRef ToRef() const
    {
        if (!value.IsValid()) {
            return ConstAnyRef();
        }

        if (const AnyRef *any_ref_ptr = value.TryGet<AnyRef>()) {
            return ConstAnyRef(*any_ref_ptr);
        }

        if (const Any *any_ptr = value.TryGet<Any>()) {
            return any_ptr->ToRef();
        }

        return ConstAnyRef(value.GetTypeID(), value.GetPointer());
    }

    template <class T>
    HYP_FORCE_INLINE bool Is() const
    {
        static_assert(!std::is_same_v<T, HypData>);

        if (!value.IsValid()) {
            return false;
        }

        // // if the struct's StorageType typedef is the same as T, we don't need to perform another check.
        // constexpr bool should_skip_additional_is_check = std::is_same_v<T, typename detail::HypDataInitializer<T>::StorageType>;
        
        // if constexpr (can_store_directly<typename detail::HypDataInitializer<T>::StorageType>) {
        //     return value.Is<typename detail::HypDataInitializer<T>::StorageType>()
        //         && (should_skip_additional_is_check || detail::HypDataInitializer<T>{}.Is(value.Get<typename detail::HypDataInitializer<T>::StorageType>()));
        // } else {
        //     if (const Any *any_ptr = value.TryGet<Any>()) {
        //         return any_ptr->Is<typename detail::HypDataInitializer<T>::StorageType>()
        //             && (should_skip_additional_is_check || detail::HypDataInitializer<T>{}.Is(any_ptr->Get<typename detail::HypDataInitializer<T>::StorageType>()));
        //     }

        //     return false;
        // }

        return detail::HypDataTypeChecker_Tuple<T, typename detail::HypDataInitializer<T>::ConvertibleTypes>{}(value);
    }

    template <class T>
    HYP_FORCE_INLINE auto Get()
    {
        static_assert(!std::is_same_v<T, HypData>);

        // if constexpr (can_store_directly<typename detail::HypDataInitializer<T>::StorageType>) {
        //     return detail::HypDataInitializer<NormalizedType<T>>{}.Get(value.Get<typename detail::HypDataInitializer<T>::StorageType>());
        // } else {
        //     return detail::HypDataInitializer<NormalizedType<T>>{}.Get(value.Get<Any>().Get<typename detail::HypDataInitializer<T>::StorageType>());
        // }

        Optional<T> result_value;

        detail::HypDataGetter_Tuple<T, T, typename detail::HypDataInitializer<T>::ConvertibleTypes> getter_instance { };

        AssertThrowMsg(getter_instance(value, result_value),
            "Failed to invoke HypData Get method with T = %s - Mismatched types or T could not be converted to the held type (current TypeID = %u)",
            TypeName<T>().Data(),
            GetTypeID().Value());

        return result_value.Get();
    }

    template <class T>
    HYP_FORCE_INLINE auto Get() const
    {
        // static_assert(!std::is_same_v<T, HypData>);

        // if constexpr (can_store_directly<typename detail::HypDataInitializer<T>::StorageType>) {
        //     return detail::HypDataInitializer<NormalizedType<T>>{}.Get(value.Get<typename detail::HypDataInitializer<T>::StorageType>());
        // } else {
        //     return detail::HypDataInitializer<NormalizedType<T>>{}.Get(value.Get<Any>().Get<typename detail::HypDataInitializer<T>::StorageType>());
        // }

        Optional<T> result_value;

        detail::HypDataGetter_Tuple<T, T, typename detail::HypDataInitializer<T>::ConvertibleTypes> getter_instance { };

        AssertThrowMsg(getter_instance(value, result_value),
            "Failed to invoke HypData Get method with T = %s - Mismatched types or T could not be converted to the held type (current TypeID = %u)",
            TypeName<T>().Data(),
            GetTypeID().Value());

        return result_value.Get();
    }
};

namespace detail {

template <class T>
struct HypDataInitializer<T, std::enable_if_t<std::is_fundamental_v<T>>>
{
    using StorageType = T;
    using ConvertibleTypes = Tuple<>;

    HYP_FORCE_INLINE bool Is(const T &value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    constexpr T Get(T value) const
    {
        return value;
    }

    void Set(HypData &hyp_data, T value) const
    {
        hyp_data.value.Set<T>(value);
    }
};

template <>
struct HypDataInitializer<IDBase>
{
    using StorageType = IDBase;
    using ConvertibleTypes = Tuple<>;

    HYP_FORCE_INLINE bool Is(const IDBase &value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    constexpr IDBase &Get(IDBase &value) const
    {
        return value;
    }

    const IDBase &Get(const IDBase &value) const
    {
        return value;
    }

    void Set(HypData &hyp_data, const IDBase &value) const
    {
        hyp_data.value.Set<IDBase>(value);
    }
};
template <class T>
struct HypDataInitializer<ID<T>> : HypDataInitializer<IDBase>
{
    using ConvertibleTypes = Tuple<>;

    HYP_FORCE_INLINE bool Is(const IDBase &value) const
    {
        return true; // can't do anything more to check as IDBase doesn't hold type info.
    }

    ID<T> &Get(IDBase &value) const
    {
        return static_cast<ID<T> &>(value);
    }

    const ID<T> &Get(const IDBase &value) const
    {
        return static_cast<const ID<T> &>(value);
    }

    void Set(HypData &hyp_data, const ID<T> &value) const
    {
        hyp_data.value.Set<IDBase>(static_cast<const IDBase &>(value));
    }
};

template <>
struct HypDataInitializer<AnyHandle>
{
    using StorageType = AnyHandle;
    using ConvertibleTypes = Tuple<>;

    HYP_FORCE_INLINE bool Is(const AnyHandle &value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    AnyHandle &Get(AnyHandle &value) const
    {
        return value;
    }

    const AnyHandle &Get(const AnyHandle &value) const
    {
        return value;
    }

    void Set(HypData &hyp_data, const AnyHandle &value) const
    {
        hyp_data.value.Set<AnyHandle>(value);
    }

    void Set(HypData &hyp_data, AnyHandle &&value) const
    {
        hyp_data.value.Set<AnyHandle>(std::move(value));
    }
};

template <class T>
struct HypDataInitializer<Handle<T>> : HypDataInitializer<AnyHandle>
{
    using ConvertibleTypes = Tuple<>;

    HYP_FORCE_INLINE bool Is(const AnyHandle &value) const
    {
        return value.type_id == TypeID::ForType<T>();
    }

    Handle<T> Get(const AnyHandle &value) const
    {
        return value.Cast<T>();
    }

    void Set(HypData &hyp_data, const Handle<T> &value) const
    {
        hyp_data.value.Set<AnyHandle>(value);
    }

    void Set(HypData &hyp_data, Handle<T> &&value) const
    {
        hyp_data.value.Set<AnyHandle>(std::move(value));
    }
};

template <>
struct HypDataInitializer<RC<void>>
{
    using StorageType = RC<void>;
    using ConvertibleTypes = Tuple<>;

    HYP_FORCE_INLINE bool Is(const RC<void> &value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    const RC<void> &Get(const RC<void> &value) const
    {
        return value;
    }

    void Set(HypData &hyp_data, const RC<void> &value) const
    {
        hyp_data.value.Set<RC<void>>(value);
    }

    void Set(HypData &hyp_data, RC<void> &&value) const
    {
        hyp_data.value.Set<RC<void>>(std::move(value));
    }
};
template <class T>
struct HypDataInitializer<RC<T>, std::enable_if_t< !std::is_void_v<T> >> : HypDataInitializer<RC<void>>
{
    HYP_FORCE_INLINE bool Is(const RC<void> &value) const
    {
        return value.Is<T>();
    }

    RC<T> Get(const RC<void> &value) const
    {
        return value.template Cast<T>();
    }

    void Set(HypData &hyp_data, const RC<T> &value) const
    {
        hyp_data.value.Set<RC<void>>(value.template Cast<void>());
    }

    void Set(HypData &hyp_data, RC<T> &&value) const
    {
        hyp_data.value.Set<RC<void>>(value.template Cast<void>());
    }
};

template <>
struct HypDataInitializer<AnyRef>
{
    using StorageType = AnyRef;
    using ConvertibleTypes = Tuple<>;

    HYP_FORCE_INLINE bool Is(const AnyRef &value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    const AnyRef &Get(const AnyRef &value) const
    {
        return value;
    }

    void Set(HypData &hyp_data, const AnyRef &value) const
    {
        hyp_data.value.Set<AnyRef>(value);
    }

    void Set(HypData &hyp_data, AnyRef &&value) const
    {
        hyp_data.value.Set<AnyRef>(std::move(value));
    }
};

template <class T>
struct HypDataInitializer<T *, std::enable_if_t< !is_const_pointer<T *> && !std::is_same_v<T *, void *> >> : HypDataInitializer<AnyRef>
{
    using ConvertibleTypes = Tuple<AnyHandle, RC<void>>;

    HYP_FORCE_INLINE bool Is(const AnyRef &value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE bool Is(const AnyHandle &value) const
    {
        return value.Is<T>();
    }

    HYP_FORCE_INLINE bool Is(const RC<void> &value) const
    {
        return value.Is<T>();
    }

    T *Get(const AnyRef &value) const
    {
        AssertThrow(value.Is<T>());

        return static_cast<T *>(value.GetPointer());
    }

    T *Get(const AnyHandle &value) const
    {
        AssertThrow(value.Is<T>());

        return value.TryGet<T>();
    }

    T *Get(const RC<void> &value) const
    {
        AssertThrow(value.Is<T>());

        return value.CastUnsafe<T>();
    }

    void Set(HypData &hyp_data, T *value) const
    {
        hyp_data.value.Set<AnyRef>(AnyRef(TypeID::ForType<T>(), value));
    }
};

template <class T>
struct HypDataInitializer<T, std::enable_if_t<!HypData::can_store_directly<T>>>
{
    using StorageType = T;
    using ConvertibleTypes = Tuple<>;

    HYP_FORCE_INLINE bool Is(const T &value) const
    {
        // should never be hit
        HYP_NOT_IMPLEMENTED();
    }

    T &Get(T &value) const
    {
        return value;
    }

    const T &Get(const T &value) const
    {
        return value;
    }

    void Set(HypData &hyp_data, const T &value) const
    {
        hyp_data.value.Set(Any(value));
    }

    void Set(HypData &hyp_data, T &&value) const
    {
        hyp_data.value.Set(Any(std::move(value)));
    }
};

#pragma region HypDataTypeChecker implementation

template <class T>
struct HypDataTypeChecker
{
    HYP_FORCE_INLINE bool operator()(const HypData::VariantType &value) const
    {
        constexpr bool should_skip_additional_is_check = std::is_same_v<T, typename HypDataInitializer<T>::StorageType>;

        if constexpr (HypData::can_store_directly<typename HypDataInitializer<T>::StorageType>) {
            return value.Is<typename HypDataInitializer<T>::StorageType>()
                && (should_skip_additional_is_check || HypDataInitializer<T>{}.Is(value.Get<typename HypDataInitializer<T>::StorageType>()));
        } else {
            if (const Any *any_ptr = value.TryGet<Any>()) {
                return any_ptr->Is<typename HypDataInitializer<T>::StorageType>()
                    && (should_skip_additional_is_check || HypDataInitializer<T>{}.Is(any_ptr->Get<typename HypDataInitializer<T>::StorageType>()));
            }

            return false;
        }
    }
};

template <class T, class... ConvertibleTypes>
struct HypDataTypeChecker_Tuple<T, Tuple<ConvertibleTypes...>>
{
    HYP_FORCE_INLINE bool operator()(const HypData::VariantType &value) const
    {
        return HypDataTypeChecker<T>{}(value) || (HypDataTypeChecker<ConvertibleTypes>{}(value) || ...);
    }
};

#pragma endregion HypDataTypeChecker implementation

#pragma region HypDataGetter implementation

// template <class ReturnType, class T>
// struct HypDataGetter
// {
//     HYP_FORCE_INLINE bool operator()(HypData::VariantType &value, ValueStorage<ReturnType> &out_storage) const
//     {
//         if constexpr (HypData::can_store_directly<typename HypDataInitializer<T>::StorageType>) {
//             return HypDataInitializer<NormalizedType<T>>{}.Get(value.Get<typename HypDataInitializer<T>::StorageType>(), out_storage);
//         } else {
//             return HypDataInitializer<NormalizedType<T>>{}.Get(value.Get<Any>().Get<typename HypDataInitializer<T>::StorageType>(), out_storage);
//         }
//     }
    
//     HYP_FORCE_INLINE bool operator()(const HypData::VariantType &value, ValueStorage<const ReturnType> &out_storage) const
//     {
//         if constexpr (HypData::can_store_directly<typename HypDataInitializer<T>::StorageType>) {
//             return HypDataInitializer<NormalizedType<T>>{}.Get(value.Get<typename HypDataInitializer<T>::StorageType>(), out_storage);
//         } else {
//             return HypDataInitializer<NormalizedType<T>>{}.Get(value.Get<Any>().Get<typename HypDataInitializer<T>::StorageType>(), out_storage);
//         }
//     }
// };


template <class ReturnType, class... Types, SizeType... Indices>
HYP_FORCE_INLINE bool HypDataGetter_Tuple_Impl(HypData::VariantType &value, Optional<ReturnType> &out_value, std::index_sequence<Indices...>)
{
    const auto InvokeGetter = []<SizeType SelectedTypeIndex>(HypData::VariantType &value, Optional<ReturnType> &out_value, std::integral_constant<SizeType, SelectedTypeIndex>) -> bool
    {
        using SelectedType = typename TupleElement<SelectedTypeIndex, Types...>::Type;

        if constexpr (HypData::can_store_directly<typename HypDataInitializer<SelectedType>::StorageType>) {
            if constexpr (std::is_same_v<ReturnType, typename HypDataInitializer<SelectedType>::StorageType>) {
                out_value.Set(value.Get<typename HypDataInitializer<SelectedType>::StorageType>());
            } else {
                decltype(auto) internal_value = value.Get<typename HypDataInitializer<SelectedType>::StorageType>();

                if (!HypDataInitializer<ReturnType>{}.Is(internal_value)) {
                    return false;
                }
                
                out_value.Set(HypDataInitializer<ReturnType>{}.Get(std::forward<decltype(internal_value)>(internal_value)));
            }
        } else {
            if constexpr (std::is_same_v<ReturnType, typename HypDataInitializer<SelectedType>::StorageType>) {
                out_value.Set(value.Get<Any>().Get<typename HypDataInitializer<SelectedType>::StorageType>());
            } else {
                decltype(auto) internal_value = value.Get<Any>().Get<typename HypDataInitializer<SelectedType>::StorageType>();

                if (!HypDataInitializer<ReturnType>{}.Is(internal_value)) {
                    return false;
                }

                out_value.Set(HypDataInitializer<ReturnType>{}.Get(std::forward<decltype(internal_value)>(internal_value)));
            }
        }

        return true;
    };

    return ((HypDataTypeChecker<Types>{}(value) && InvokeGetter(value, out_value, std::integral_constant<SizeType, Indices>{})) || ...);
}

template <class ReturnType, class... Types, SizeType... Indices>
HYP_FORCE_INLINE bool HypDataGetter_Tuple_Impl(const HypData::VariantType &value, Optional<ReturnType> &out_value, std::index_sequence<Indices...>)
{
    const auto InvokeGetter = []<SizeType SelectedTypeIndex>(const HypData::VariantType &value, Optional<ReturnType> &out_value, std::integral_constant<SizeType, SelectedTypeIndex>) -> bool
    {
        using SelectedType = typename TupleElement<SelectedTypeIndex, Types...>::Type;

        if constexpr (HypData::can_store_directly<typename HypDataInitializer<SelectedType>::StorageType>) {
            out_value.Set(HypDataInitializer<ReturnType>{}.Get(value.Get<typename HypDataInitializer<SelectedType>::StorageType>()));
        } else {
            out_value.Set(HypDataInitializer<ReturnType>{}.Get(value.Get<Any>().Get<typename HypDataInitializer<SelectedType>::StorageType>()));
        }

        return true;
    };

    return ((HypDataTypeChecker<Types>{}(value) && InvokeGetter(value, out_value, std::integral_constant<SizeType, Indices>{})) || ...);
}

template <class ReturnType, class T, class... ConvertibleTypes>
struct HypDataGetter_Tuple<ReturnType, T, Tuple<ConvertibleTypes...>>
{
    HYP_FORCE_INLINE bool operator()(HypData::VariantType &value, Optional<ReturnType> &out_value) const
    {
        return HypDataGetter_Tuple_Impl<ReturnType, T, ConvertibleTypes...>(value, out_value, std::index_sequence_for<T, ConvertibleTypes...>{});
    }

    HYP_FORCE_INLINE bool operator()(const HypData::VariantType &value, Optional<ReturnType> &out_value) const
    {
        return HypDataGetter_Tuple_Impl<ReturnType, T, ConvertibleTypes...>(value, out_value, std::index_sequence_for<T, ConvertibleTypes...>{});
    }
};

#pragma endregion HypDataGetter implementation

} // namespace detail

static_assert(sizeof(HypData) == 32, "sizeof(HypData) must match C# struct size");

} // namespace hyperion

#endif