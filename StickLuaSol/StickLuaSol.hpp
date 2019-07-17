#ifndef STICKLUASOL_STICKLUASOL_HPP
#define STICKLUASOL_STICKLUASOL_HPP

#include <Stick/DynamicArray.hpp>
#include <Stick/Maybe.hpp>
#include <Stick/Path.hpp>
#include <Stick/Result.hpp>
#include <Stick/SharedPtr.hpp>
#include <Stick/String.hpp>
#include <Stick/TypeList.hpp>
#include <Stick/Variant.hpp>

#include <sol/sol.hpp>

//@NOTE: This is by no means a complete binding for stick but rather a bunch of template
// specializations for sol to implicitly push basic stick types between lua/c++.

namespace stickLuaSol
{
STICK_API inline sol::table ensureNamespaceTable(sol::state_view _lua,
                                                 sol::table _startTbl,
                                                 const stick::String & _namespace)
{
    using namespace stick;

    sol::table tbl = _startTbl;
    if (!_namespace.isEmpty())
    {
        auto tokens = path::segments(_namespace, '.');
        for (const String & token : tokens)
            tbl = tbl[token.cString()] = tbl.get_or(token.cString(), _lua.create_table());
    }
    return tbl;
}
} // namespace stickLuaSol

//@TODO add missing conversions for i.e. unique ptr etc.?
namespace sol
{
template <class T>
struct unique_usertype_traits<stick::SharedPtr<T>>
{
    typedef T type;
    typedef stick::SharedPtr<T> actual_type;
    static const bool value = true;

    static bool is_null(const actual_type & value)
    {
        return (bool)!value;
    }

    static type * get(const actual_type & p)
    {
        return p.get();
    }
};

template <class T, class C>
struct unique_usertype_traits<stick::UniquePtr<T, C>>
{
    typedef T type;
    typedef stick::UniquePtr<T, C> actual_type;
    static const bool value = true;
    using rebind_base = void;

    static bool is_null(const actual_type & value)
    {
        return (bool)!value;
    }

    static type * get(const actual_type & p)
    {
        return p.get();
    }
};

template <>
struct lua_size<stick::Error> : std::integral_constant<int, 1>
{
};

template <>
struct lua_type_of<stick::Error> : std::integral_constant<sol::type, sol::type::table>
{
};

namespace stack
{
template <>
struct checker<stick::Error>
{
    template <typename Handler>
    static bool check(lua_State * L, int index, Handler && handler, record & tracking)
    {
        int idx = lua_absindex(L, index);
        tracking.use(1);
        if (!sol::stack::check<sol::nil_t>(L, idx) && !sol::stack::check<sol::table>(L, idx))
        {
            handler(
                L, idx, type_of(L, idx), type::poly, "Expected nil or table to convert to Error.");
            return false;
        }
        return true;
    }
};

template <>
struct getter<stick::Error>
{
    static stick::Error get(lua_State * L, int index, record & tracking)
    {
        int absolute_index = lua_absindex(L, index);
        if (sol::stack::check<sol::nil_t>(L, absolute_index))
            return stick::Error();

        sol::table tbl(L, index);
        tracking.use(1);
        return stick::Error(tbl.get<stick::Int32>("code"),
                            *tbl.get<stick::ErrorCategory *>("category"),
                            tbl.get<const char *>("message"),
                            tbl.get<const char *>("file"),
                            tbl.get<stick::Size>("line"));
    }
};

template <>
struct pusher<stick::Error>
{
    static int push(lua_State * L, const stick::Error & _err)
    {
        if (_err)
        {
            sol::table tbl(L, sol::new_table(0, 6));

            tbl["message"] = _err.message().cString();
            tbl["code"] = (stick::Size)_err.code();
            tbl["category"] = &_err.category();
            tbl["file"] = _err.file().cString();
            tbl["line"] = _err.line();
            tbl["description"] = _err.description().cString();

            sol::stack::push(L, tbl);
        }
        else
        {
            sol::stack::push(L, sol::nil_t());
        }

        return 1;
    }
};
} // namespace stack

template <>
struct lua_size<stick::String> : std::integral_constant<int, 1>
{
};

template <>
struct lua_type_of<stick::String> : std::integral_constant<sol::type, sol::type::string>
{
};

namespace stack
{
template <>
struct checker<stick::String>
{
    template <typename Handler>
    static bool check(lua_State * L, int index, Handler && handler, record & tracking)
    {
        return sol::stack::check<const char *>(L, lua_absindex(L, index), handler, tracking);
    }
};

template <>
struct getter<stick::String>
{
    static stick::String get(lua_State * L, int index, record & tracking)
    {
        tracking.use(1);
        std::size_t len;
        auto str = lua_tolstring(L, index, &len);
        return stick::String(str, str + len);
    }
};

template <>
struct pusher<stick::String>
{
    static int push(lua_State * L, const stick::String & _str)
    {
        if (_str.isEmpty())
            sol::stack::push(L, "");
        else
            sol::stack::push(L, _str.cString());
        return 1;
    }
};
} // namespace stack

template <class T>
struct lua_size<stick::Maybe<T>> : std::integral_constant<int, 1>
{
};

template <class T>
struct lua_type_of<stick::Maybe<T>> : std::integral_constant<sol::type, sol::type::poly>
{
};

namespace stack
{
template <class T>
struct checker<stick::Maybe<T>, sol::type::poly>
{
    template <typename Handler>
    static bool check(lua_State * L, int index, Handler && handler, record & tracking)
    {
        int idx = lua_absindex(L, index);
        tracking.use(1);
        if (!sol::stack::check<sol::nil_t>(L, idx) && !sol::stack::check<T>(L, idx))
        {
            handler(
                L, idx, type_of(L, idx), type::poly, "Expected nil or T to convert to Maybe<T>.");
            return false;
        }

        tracking.use(1);
        return true;
    }
};

template <class T>
struct getter<stick::Maybe<T>>
{
    static stick::Maybe<T> get(lua_State * L, int index, record & tracking)
    {
        int absolute_index = lua_absindex(L, index);
        tracking.use(1);
        if (sol::stack::check<sol::nil_t>(L, absolute_index))
            return stick::Maybe<T>();
        return sol::stack::get<T>(L, absolute_index);
    }
};

template <class T>
struct pusher<stick::Maybe<T>>
{
    static int push(lua_State * L, const stick::Maybe<T> & _maybe)
    {
        if (!_maybe)
            sol::stack::push(L, sol::nil_t());
        else
            sol::stack::push(L, *_maybe);
        return 1;
    }
};
} // namespace stack

template <class... Args>
struct lua_size<stick::Variant<Args...>> : std::integral_constant<int, 1>
{
};

template <class... Args>
struct lua_type_of<stick::Variant<Args...>> : std::integral_constant<sol::type, sol::type::poly>
{
};

namespace stack
{
template <class... Args>
struct checker<stick::Variant<Args...>, sol::type::poly>
{
    using VariantType = stick::Variant<Args...>;
    using TL = typename stick::MakeTypeList<Args...>::List;

    template <typename Handler>
    static bool is_one(std::integral_constant<std::size_t, 0>,
                       lua_State * L,
                       int index,
                       Handler && handler,
                       record & tracking)
    {
        tracking.use(1);
        handler(L,
                index,
                type::poly,
                type_of(L, index),
                "value does not fit any type present in the stick::Variant");
        return false;
    }

    template <std::size_t I, typename Handler>
    static bool is_one(std::integral_constant<std::size_t, I>,
                       lua_State * L,
                       int index,
                       Handler && handler,
                       record & tracking)
    {
        using Type = typename stick::TypeAt<TL, I - 1>::Type;
        if (stack::check<Type>(L, index, no_panic, tracking))
            return true;
        return is_one(std::integral_constant<std::size_t, I - 1>(),
                      L,
                      index,
                      std::forward<Handler>(handler),
                      tracking);
    }

    template <typename Handler>
    static bool check(lua_State * L, int index, Handler && handler, record & tracking)
    {
        int idx = lua_absindex(L, index);
        if (sol::stack::check<sol::nil_t>(L, idx))
        {
            tracking.use(1);
            return true;
        }
        return is_one(std::integral_constant<std::size_t, TL::count>(),
                      L,
                      index,
                      std::forward<Handler>(handler),
                      tracking);
    }
};

template <class... Args>
struct getter<stick::Variant<Args...>>
{
    using VariantType = stick::Variant<Args...>;
    using TL = typename stick::MakeTypeList<Args...>::List;

    static VariantType get_one(std::integral_constant<std::size_t, 0>,
                               lua_State * L,
                               int index,
                               record & tracking)
    {
        return VariantType();
    }

    template <std::size_t I>
    static VariantType get_one(std::integral_constant<std::size_t, I>,
                               lua_State * L,
                               int index,
                               record & tracking)
    {
        using Type = typename stick::TypeAt<TL, I - 1>::Type;
        //@TODO: Not sure if we need to add someting like std::in_place_index_t to stick::Variant
        // hmm
        if (stack::check<Type>(L, index, no_panic, tracking))
            return stack::get<Type>(L, index);
        return get_one(std::integral_constant<std::size_t, I - 1>(), L, index, tracking);
    }

    static stick::Variant<Args...> get(lua_State * L, int index, record & tracking)
    {
        return get_one(std::integral_constant<std::size_t, TL::count>(), L, index, tracking);
    }
};

template <class... Args>
struct pusher<stick::Variant<Args...>>
{
    using VariantType = stick::Variant<Args...>;
    using TL = typename stick::MakeTypeList<Args...>::List;

    template <class VT>
    static int push_one(std::integral_constant<std::size_t, 0>, lua_State * L, VT && _variant)
    {
        return sol::stack::push(L, sol::nil_t());
    }

    template <class VT, std::size_t I>
    static int push_one(std::integral_constant<std::size_t, I>, lua_State * L, VT && _variant)
    {
        using Type = typename stick::TypeAt<TL, I - 1>::Type;
        if (_variant.template is<Type>())
            return sol::stack::push(L, _variant.template get<Type>());
        else
            return push_one(
                std::integral_constant<std::size_t, I - 1>(), L, std::forward<VT>(_variant));
    }

    static int push(lua_State * L, const VariantType & _variant)
    {
        return push_one(std::integral_constant<std::size_t, TL::count>(), L, _variant);
    }

    static int push(lua_State * L, VariantType && _variant)
    {
        return push_one(std::integral_constant<std::size_t, TL::count>(), L, std::move(_variant));
    }
};
} // namespace stack

template <class T>
struct lua_size<stick::Result<T>> : std::integral_constant<int, 1>
{
};

template <class T>
struct lua_type_of<stick::Result<T>> : std::integral_constant<sol::type, sol::type::poly>
{
};

namespace stack
{
template <class T>
struct pusher<stick::Result<T>>
{
    static int push(lua_State * L, const stick::Result<T> & _result)
    {
        // sol::table tbl(L, sol::new_table(0, 1));
        // tbl.set_function("ensure", [_result]() { return std::move(_result.ensure()); });
        // tbl.set_function("get", [L, _result]() {
        //     if (_result)
        //         return sol::make_object(L, _result.get());
        //     else
        //         return sol::make_object(L, _result.error());
        // });
        // sol::stack::push(L, tbl);
        // return 1;

        if(_result)
        {
            // return sol::make_object(L, std::move(_result.get()));
            sol::stack::push(L, std::move(_result.get()));
        }
        else
        {
            // return sol::make_object(L, _result.error());
            sol::stack::push(L, _result.error());
        }

        return 1;
    }
};
} // namespace stack

template <class T>
struct is_container<stick::DynamicArray<T>> : std::true_type
{
};

} // namespace sol

#endif // STICKLUASOL_STICKLUASOL_HPP
