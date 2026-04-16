#pragma once

#include "dib/debug.h"
#include "dib/fn.h"
#include "dib/types.h"
#include "dib/option.h"
#include "dib/preprocess.h"
#include "dib/metautils.h"
#include "dib/record.h"
#include "dib/metafunction.h"
#include "dib/conststring.h"

#include <ranges>
#include <span>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <functional>
#include <meta>

namespace dib::refl
{
    struct Type;
    struct TypeStorage;
    struct Definition;
    struct Field;
    struct Method;
    struct Overload;
    class Any;
    class AnyRef;

    class OmitFromReflection {};
    constexpr OmitFromReflection omit;

    class ReflectAll {};
    constexpr ReflectAll all;

    template<dib::strings::StringConst Typename, dib::strings::StringConst MethodName>
    struct UnsafeForReflection;

    template<dib::strings::StringConst TypeName, dib::strings::StringConst MethodName>
    consteval void mark_unsafe_for_reflection()
    {
        std::meta::define_aggregate(
            std::meta::substitute(^^UnsafeForReflection, { std::meta::reflect_constant(TypeName), std::meta::reflect_constant(MethodName) }), {});
    }
    
    template<dib::strings::StringConst TypeName, dib::strings::StringConst MethodName>
    consteval bool is_unsafe_for_reflection()
    {
        return std::meta::is_complete_type(^^UnsafeForReflection<TypeName, MethodName>);
    }

    enum class ReflectionDepth
    {
        none,
        public_api,
        all
    };
    
    struct [[=omit]] ReflectionImpl
    {
        template<std::meta::info I>
        consteval static void finalize_definition(Definition &def);
        
        template<class T>
        consteval static TypeStorage compute_type_storage();
        
        template<class T>
        static const Type *compute_type_ptr();

        template<class T>
        constexpr static Type compute_type();

        template<class T, std::meta::info FieldInfo>
        constexpr static Field compute_field();
        
        template<class T, std::meta::info MethodInfo>
        constexpr static Overload compute_overload();
    };

    /// The core type of the reflection system; provides all
    /// external APIs for types.
    struct [[=hash_by_key, =omit]] Type
    {
    private:
        const TypeStorage *v;
        bool _is_const : 1;
        bool _is_volatile : 1;

        friend struct ReflectionImpl;

    public:
        constexpr Type()
            : v(nullptr), _is_const(false), _is_volatile(false)
        {}
        constexpr Type(const TypeStorage *type, bool is_const, bool is_volatile)
            : v(type), _is_const(is_const), _is_volatile(is_volatile)
        {}

        constexpr Type remove_const() const { return Type(v, false, _is_volatile); }
        constexpr Type remove_volatile() const { return Type(v, _is_const, false); }
        constexpr Type remove_cv() const { return Type(v, false, false); }

        constexpr Type add_const() const { return Type(v, true, _is_volatile); }
        constexpr Type add_volatile() const { return Type(v, _is_const, true); }

        constexpr bool is_valueless() const { return v == nullptr; }
        
        constexpr bool is_const() const { return _is_const; }
        constexpr bool is_volatile() const { return _is_volatile; }

        constexpr bool is_pointer() const;
        constexpr bool is_reference() const;
        constexpr bool is_lv_reference() const;
        constexpr bool is_rv_reference() const;

        constexpr std::string_view name() const;
        constexpr dib::option::Option<Type> pointee() const;

        constexpr size_t size() const;
        constexpr size_t packed_size() const;
        constexpr size_t align() const;
        
        constexpr bool is_polymorphic() const;
        constexpr bool is_trivial() const;
        constexpr bool is_trivially_copyable() const;
        constexpr bool is_trivially_movable() const;
        constexpr bool is_trivially_relocatable() const;

        constexpr bool can_compare_eq() const;
        constexpr bool can_compare() const;
        constexpr bool can_hash() const;

		constexpr bool can_default() const;
		constexpr bool can_move() const;
		constexpr bool can_copy() const;
		constexpr bool can_swap() const;
        constexpr bool can_relocate() const;

        constexpr void destruct(void *dest) const;
        constexpr void delete_object(void *dest) const;
        constexpr void delete_array(void *dest) const;
        constexpr void *new_object() const;
        constexpr void *new_array(size_t size) const;

		constexpr void uninitialized_default_construct(void *dest) const;
		constexpr void uninitialized_move_construct(void *src, void *dest) const;
		constexpr void uninitialized_copy_construct(void *src, void *dest) const;
		constexpr void uninitialized_relocate(void *src, void *dest) const;

		constexpr void default_construct(void *dest) const;
		constexpr void move_construct(void *src, void *dest) const;
		constexpr void copy_construct(void *src, void *dest) const;
		constexpr void relocate(void *src, void *dest) const;

		constexpr void swap(void *src, void *dest) const;

		constexpr size_t hash(void *src) const;
		
		constexpr bool eq(void *lhs, void *rhs) const;
		constexpr bool noteq(void *lhs, void *rhs) const;
		constexpr bool less(void *lhs, void *rhs) const;
		constexpr bool lesseq(void *lhs, void *rhs) const;
		constexpr bool greater(void *lhs, void *rhs) const;
		constexpr bool greatereq(void *lhs, void *rhs) const;

        constexpr ReflectionDepth reflection_depth() const;
        
        constexpr std::span<const AnyRef> attributes() const;
        constexpr std::span<const Field> fields() const;
        constexpr std::span<const Method> methods() const;
        constexpr std::span<const Type *const> bases() const;

        constexpr bool has_attribute_of_type(Type type, bool include_bases = true) const;

        constexpr dib::option::Option<const Field &> resolve_field(std::string_view name, bool include_bases = true) const;
        constexpr dib::option::Option<const Method &> resolve_method(std::string_view name, bool include_bases = true) const;

        size_t get_hash() const;
        constexpr bool operator==(const Type &other) const;
    };

    // TODO: implement custom Any //
    enum class ConstBehaviour
    {
        include,
        ignore
    };

    template<class Self>
    class [[=omit]] AnyBase
    {
    private:
        constexpr Self *self() { return (Self *)this; }
        constexpr const Self *self() const { return (const Self *)this; }

        constexpr static Type apply_cb(Type t, ConstBehaviour cb)
        {
            if(cb == ConstBehaviour::ignore) return t.remove_cv();
            return t;
        }

    public:
        constexpr Type type() const { return self()->_type; }
        constexpr void *value() { return self()->_ptr; }
        constexpr const void *value() const { return self()->_ptr; }

        constexpr bool is_empty() const { return self()->_ptr == nullptr; }

        template<class T, ConstBehaviour C = ConstBehaviour::include>
        constexpr auto as() -> decltype(auto)
        {
            if(is_empty()) [[unlikely]]
                RUNTIME_ERROR("Cannot read from an empty Any");

            if(apply_cb(self()->_type, C) != apply_cb(ReflectionImpl::compute_type<T>(), C)) [[unlikely]]
                RUNTIME_ERROR("Cannot read an Any of type {} as a {}", self()->_type.name(), ReflectionImpl::compute_type<T>().name());

            if constexpr(std::is_reference_v<T>)
            {
                return (T)(*(std::remove_cvref_t<T> *)self()->_ptr);
            }
            else
            {
                return *(T *)self()->_ptr;
            }
        }

        template<class T, ConstBehaviour C = ConstBehaviour::include>
        constexpr auto as() const
        {
            if(is_empty()) [[unlikely]]
                RUNTIME_ERROR("Cannot read from an empty Any");

            if(apply_cb(self()->_type, C) != apply_cb(ReflectionImpl::compute_type<T>(), C)) [[unlikely]]
                RUNTIME_ERROR("Cannot read an Any of type {} as a {}", self()->_type.name(), ReflectionImpl::compute_type<T>().name());

            if constexpr(std::is_reference_v<T>)
            {
                return static_cast<T>(*(std::remove_cv<T> *)self()->_ptr);
            }
            else
            {
                return *(const T *)self()->_ptr;
            }
        }

        constexpr size_t get_hash() const { return self()->_type.hash(self()->_ptr); }

        constexpr bool operator==(const Self &other) const
        {
            if(self()->_type != other._type)
                RUNTIME_ERROR("Cannot compare two AnyRef instances of distinct types {} and {}", self()->_type.name(), other._type.name());

            return self()->_type.eq(self()->_ptr, other._ptr);
        }

        constexpr std::weak_ordering operator<=>(const Self &other) const
        {
            if(self()->_type != other._type)
                RUNTIME_ERROR("Cannot compare two AnyRef instances of distinct types {} and {}", self()->_type.name(), other._type.name());

            if(self()->_type.less(self()->_ptr, other._ptr))
            {
                return std::weak_ordering::less;
            }
            else if(self()->_type.greater(self()->_ptr, other._ptr))
            {
                return std::weak_ordering::greater;
            }

            return std::weak_ordering::equivalent;
        }
    };

    class [[=provides_hash, =omit]] AnyRef : public AnyBase<AnyRef>
    {
    private:
        Type _type;
        void *_ptr;

        friend class AnyBase<AnyRef>;
        
        template<class T>
        using FromArg = typename [:
            []{
                if constexpr(std::meta::is_reference_type(^^T))
                {
                    return ^^T;
                }
                else
                {
                    return ^^T&;
                }
            }()
        :];

    public:
        constexpr AnyRef() 
            : _type()
            , _ptr(nullptr)
        {}
        constexpr AnyRef(Type type, void *ptr)
            : _type(type)
            , _ptr(ptr)
        {}

        constexpr AnyRef(const AnyRef &other)
            : _type(other._type)
            , _ptr(other._ptr)
        {}

        constexpr AnyRef &operator=(const AnyRef &other)
        {
            _type = other._type;
            _ptr = other._ptr;

            return *this;
        }

        template<types::NotCVRefEq<AnyRef> T>
        static AnyRef from(FromArg<T> arg)
        {
            return AnyRef(
                *ReflectionImpl::compute_type_ptr<T>(), 
                (void*)&(arg));
        }
    };

    class [[=provides_hash, =omit]] Any : public AnyBase<Any>
    {
    private:
        Type _type;
        void *_ptr = nullptr;

        // TODO: small object optimization //

        friend class AnyBase<Any>;

        constexpr void destroy()
        {
            if(_ptr)
            {
                _type.delete_object(_ptr);
            }
        }
        
        constexpr void move_from(Any &&other)
        {
            _type = other._type;
            _ptr = other._ptr;
            
            other._ptr = nullptr;
        }

        constexpr Any(Type type, void *ptr)
            : _type(type), _ptr(ptr)
        {}
        
        template<class T>
        struct FromArgType { using Type = const T &; };
        template<class T>
        struct FromArgType<T&&> { using Type = T&&; };
        template<class T>
        struct FromArgType<T&> { using Type = T&; };

    public:
        constexpr Any()
            : _type(), _ptr(nullptr)
        {}
        constexpr Any(Type type)
            : _type(type), _ptr(type.new_object())
        {}

        constexpr ~Any()
        {
            if(_ptr)
            {
                _type.delete_object(_ptr);
            }
        }

        constexpr Any(Any &&other)
        {
            move_from(MOVE(other));
        }

        constexpr Any &operator=(Any &&other)
        {
            destroy();
            move_from(MOVE(other));
            return *this;
        }

        template<class T>
        constexpr static Any from(FromArgType<T>::Type argument)
        {   
            using S = std::remove_cvref_t<T>;
            S *value = new S(argument);

            return Any(ReflectionImpl::compute_type<T>(), value);
        }
        
        constexpr static Any clone(AnyRef ref)
        {
            Any result(ref.type());
            result._type.move_construct(ref.value(), result.value());

            return result;
        }

        constexpr AnyRef ref()
        {
            return AnyRef(_type, _ptr);
        }
        constexpr const AnyRef ref() const
        {
            return AnyRef(_type, _ptr);
        }
    };

    struct [[=omit]] Definition
    {
    private:
        std::string_view _name;
        std::span<const AnyRef> _attributes;

        bool _is_private : 1;
        bool _is_protected : 1;

        friend struct ReflectionImpl;
        friend struct Type;

    public:
        constexpr std::string_view name() const { return _name; }

        constexpr std::span<const AnyRef> attributes() const { return _attributes; }
        constexpr bool has_attribute_of_type(Type type) const
        {
            return std::ranges::any_of(_attributes, [&](auto &&x) { return x.type() == type; });
        }

        constexpr bool is_private() const { return _is_private; }
        constexpr bool is_protected() const { return _is_protected; }
    };

    struct [[=omit]] Field : public Definition
    {
    private:
        const Type *_type;

        functional::fn<AnyRef(AnyRef src)> _ref;
        functional::fn<Any(AnyRef src)> _get;
        functional::fn<void (AnyRef src, AnyRef val)> _set;

        bool _is_static : 1;
        
        friend struct ReflectionImpl;
        friend struct Type;
    
    public:
        constexpr Type type() const { return *_type; }

        constexpr bool can_ref() const { return _ref; }
        constexpr bool can_set() const { return _set; }
        constexpr bool can_get() const { return _get; }

        constexpr AnyRef ref(AnyRef object) const 
        { 
            if(!can_ref())
                RUNTIME_ERROR("Cannot get a reference to Field {} of type {}", type().name(), name());

            return _ref(object); 
        }
        constexpr Any get(AnyRef object) const 
        { 
            if(!can_get())
                RUNTIME_ERROR("Cannot get a copy of Field {} {}", type().name(), name());

            return _get(object); 
        }
        constexpr void set(AnyRef object, AnyRef val) const 
        {
            if(!can_set())
                RUNTIME_ERROR("Cannot set Field {} {}", type().name(), name());

            _set(object, val); 
        }
    };

    struct [[=omit]] Overload : public Definition
    {
    private:
        std::span<const Type *const> _argument_types;
        const Type *_return_type;
        
        bool _is_const : 1;
        bool _is_virtual : 1;
        bool _is_static : 1;
        
        functional::fn<
            void (AnyRef self, std::span<AnyRef> args, AnyRef &ret)> _wrapper;
        
        friend struct ReflectionImpl;
        friend struct Type;

    public:
        constexpr std::span<const Type *const> argument_types() const { return _argument_types; }
        constexpr Type return_type() const { return *_return_type; }

        constexpr bool is_const() const { return _is_const; }
        constexpr bool is_static() const { return _is_static; }
        constexpr bool is_virtual() const { return _is_virtual; }

        constexpr void invoke(AnyRef self, std::span<AnyRef> args, AnyRef &ret) const
        {
            _wrapper(self, args, ret);
        }
    };

    struct [[=omit]] Method
    {
    private:
        
        std::span<const Overload> _overloads;
        std::string_view _name;
        
        friend struct ReflectionImpl;
        friend struct Type;

        constexpr const Overload &overload_for_arguments(std::span<AnyRef> arguments) const
        {
            /// Common case; one overload - assume this is the one we want ///
            if(_overloads.size() == 1) [[likely]]
                return _overloads[0];

            /// Iterate overloads; look for matching arguments ///
            for(auto &overload : _overloads)
            {
                auto arg_types = overload.argument_types();
                auto match = true;

                for(auto i = 0uz; i < arg_types.size(); i++)
                {
                    if(*arg_types[i] != arguments[i].type())
                    {
                        match = false;
                        break;
                    }
                }

                if(match)
                {
                    return overload;
                }
            }

            /// Get a nice format for a pretty error ///
            auto types = std::string("");
            auto write_comma = false;

            for(auto arg : arguments)
            {
                if(write_comma)
                {
                    types += ", ";
                }
                else
                {
                    write_comma = true;
                }

                types += arg.type().name();
            }
            
            RUNTIME_ERROR("Cannot find matching overload of {} for types {}", name(), types);
        }

    public:
        constexpr std::string_view name() const { return _name; }
        constexpr std::span<const Overload> overloads() const { return _overloads; }
        
        constexpr void invoke(AnyRef self, std::span<AnyRef> args, AnyRef &ret) const
        {
            overload_for_arguments(args).invoke(self, args, ret);
        }
    };

    /// The "meat" of the Type class; the fields that actually define a
    /// type and its guts/inner workings.
    struct [[=provides_hash, =omit]] TypeStorage : public Definition
    {
    private:
        // Reflection information //
        ReflectionDepth _reflection_depth = ReflectionDepth::none;

        // Type identification //
        const std::type_info *_rtti = nullptr;

        // Type members //
        std::span<const Field> _fields;
        std::span<const Method> _methods;
        std::span<const Type *const> _bases;

        // Type operators //
        void (*_delete)(void *) = nullptr;
        void (*_delete_array)(void *) = nullptr;
        void *(*_new)() = nullptr;
        void *(*_new_array)(size_t) = nullptr;

        void (*_destruct)(void *) = nullptr;
        void (*_default_construct)(void *) = nullptr;
        void (*_relocate)(void *src, void *dest) = nullptr;
        void (*_move_construct)(void *src, void *dest) = nullptr;
        void (*_copy_construct)(void *src, void *dest) = nullptr;
        void (*_swap)(void *src, void *dest) = nullptr;

        size_t (*_hash)(void *) = nullptr;
        bool (*_eq)(void *lhs, void *rhs) = nullptr;
        bool (*_less)(void *lhs, void *rhs) = nullptr;
        
        // Type layout //
        size_t _size = 0;
        size_t _packed_size = 0;
        size_t _align = 0;

        // Type traits //
        bool _polymorphic : 1 = false;
        bool _trivial : 1 = false;
        bool _trivial_copy : 1 = false;
        bool _trivial_dest : 1 = false;
        bool _trivial_move : 1 = false;
        bool _trivial_rel : 1 = false;

        // Type structure //
        bool _is_pointer : 1 = false;
        bool _is_lv_reference : 1 = false;
        bool _is_rv_reference : 1 = false;
        
        const Type *_pointee = nullptr;
        
        std::span<const Type *const> _arguments;
        const Type *_return = nullptr;
        
        friend struct ReflectionImpl;
        friend struct Type;

    public:
        size_t get_hash() const
        {
            return _rtti->hash_code();
        }

        constexpr bool operator==(const TypeStorage &other) const
        {
            return *_rtti == *other._rtti;
        }
    };

    //#region Type Methods
        constexpr bool Type::is_pointer() const { return v->_is_pointer; }
        constexpr bool Type::is_reference() const { return v->_is_lv_reference || v->_is_rv_reference; }
        constexpr bool Type::is_lv_reference() const { return v->_is_lv_reference; }
        constexpr bool Type::is_rv_reference() const { return v->_is_rv_reference; }

        constexpr std::string_view Type::name() const { return v->name(); }
        constexpr ReflectionDepth Type::reflection_depth() const { return v->_reflection_depth; }
        
        constexpr dib::option::Option<Type> Type::pointee() const 
        {
            if(is_pointer() || is_reference())
                return Type(*v->_pointee);

            return dib::option::none;
        }

        constexpr size_t Type::size() const { return v->_size; }
        constexpr size_t Type::packed_size() const { return v->_packed_size; }
        constexpr size_t Type::align() const { return v->_align; }
        
        constexpr bool Type::is_polymorphic() const { return v->_polymorphic; }
        constexpr bool Type::is_trivial() const { return v->_trivial; }
        constexpr bool Type::is_trivially_copyable() const { return v->_trivial_copy; }
        constexpr bool Type::is_trivially_movable() const { return v->_trivial_move; }
        constexpr bool Type::is_trivially_relocatable() const { return v->_trivial_rel; }

        constexpr bool Type::can_compare_eq() const { return v->_eq; }
        constexpr bool Type::can_compare() const { return v->_less; }
        constexpr bool Type::can_hash() const { return v->_hash; }

		constexpr bool Type::can_default() const { return v->_default_construct; }
		constexpr bool Type::can_move() const { return v->_move_construct; }
		constexpr bool Type::can_copy() const { return v->_copy_construct; }
		constexpr bool Type::can_swap() const { return v->_swap; }
        constexpr bool Type::can_relocate() const { return v->_relocate; }
        
        constexpr void Type::delete_object(void *dest) const { v->_delete(dest); }
        constexpr void Type::delete_array(void *dest) const { v->_delete_array(dest); }
        constexpr void *Type::new_object() const 
        { 
            if(!can_default())
                RUNTIME_ERROR("Cannot default construct a value of type {}", name());
            
            return v->_new(); 
        }
        constexpr void *Type::new_array(size_t count) const
        { 
            if(!can_default())
                RUNTIME_ERROR("Cannot default construct a value of type {}", name());
            
            return v->_new_array(count); 
        }

        constexpr void Type::destruct(void *dest) const { v->_destruct(dest); }

		constexpr void Type::uninitialized_default_construct(void *dest) const 
        { 
            if(!can_default())
                RUNTIME_ERROR("Cannot default construct a value of type {}", name());

            v->_default_construct(dest); 
        }
		constexpr void Type::uninitialized_move_construct(void *src, void *dest) const 
        { 
            if(!can_move())
                RUNTIME_ERROR("Cannot move construct a value of type {}", name());
            
            v->_move_construct(src, dest); 
        }
		constexpr void Type::uninitialized_copy_construct(void *src, void *dest) const 
        { 
            if(!can_copy())
                RUNTIME_ERROR("Cannot copy construct a value of type {}", name());

            v->_copy_construct(src, dest); 
        }
		constexpr void Type::uninitialized_relocate(void *src, void *dest) const 
        { 
            if(!can_relocate())
                RUNTIME_ERROR("Cannot relocate a value of type {}", name());

            v->_relocate(src, dest); 
        }

		constexpr void Type::default_construct(void *dest) const { destruct(dest); uninitialized_default_construct(dest); }
		constexpr void Type::move_construct(void *src, void *dest) const { destruct(dest); uninitialized_move_construct(src, dest); }
		constexpr void Type::copy_construct(void *src, void *dest) const { destruct(dest); uninitialized_copy_construct(src, dest); }
		constexpr void Type::relocate(void *src, void *dest) const { destruct(dest); uninitialized_relocate(src, dest); }

		constexpr void Type::swap(void *src, void *dest) const 
        { 
            if(!can_swap())
                RUNTIME_ERROR("Cannot swap a value of type {}", name());

            v->_swap(src, dest); 
        }

		constexpr size_t Type::hash(void *src) const
        {
            if(!v->_hash)
                RUNTIME_ERROR("Cannot hash a value of type {}", name());

            return v->_hash(src);
        }
		
		constexpr bool Type::eq(void *lhs, void *rhs) const
        {
            if(!can_compare_eq())
                RUNTIME_ERROR("Cannot equality compare a value of type {}", name());

            return v->_eq(lhs, rhs);
        }
		constexpr bool Type::noteq(void *lhs, void *rhs) const { return !eq(lhs, rhs); }
		constexpr bool Type::less(void *lhs, void *rhs) const
        {
            if(!can_compare())
                RUNTIME_ERROR("Cannot compare a value of type {}", name());

            return v->_less(lhs, rhs);
        }
		constexpr bool Type::lesseq(void *lhs, void *rhs) const { return less(lhs, rhs) || eq(lhs, rhs); }
		constexpr bool Type::greater(void *lhs, void *rhs) const { return less(rhs, lhs); }
		constexpr bool Type::greatereq(void *lhs, void *rhs) const { return lesseq(rhs, lhs); }
        
        constexpr std::span<const AnyRef> Type::attributes() const { return v->_attributes; }
        constexpr std::span<const Field> Type::fields() const { return v->_fields; }
        constexpr std::span<const Method> Type::methods() const { return v->_methods; }
        constexpr std::span<const Type *const> Type::bases() const { return v->_bases; }

        constexpr bool Type::has_attribute_of_type(Type type, bool include_bases) const
        {
            for(auto &attr : attributes())
                if(attr.type() == type)
                    return true;

            if(include_bases)
                for(auto &base : bases())
                    if(base->has_attribute_of_type(type))
                        return true;

            return false;
        }

        constexpr dib::option::Option<const Field &> Type::resolve_field(std::string_view name, bool include_bases) const
        {
            for(auto &field : fields())
                if(field.name() == name)
                    return field;

            if(include_bases)
                for(auto &base : bases())
                    if(auto field = base->resolve_field(name); field.has_value())
                        return field;

            return dib::option::none;
        }

        constexpr dib::option::Option<const Method &> Type::resolve_method(std::string_view name, bool include_bases) const
        {
            for(auto &method : methods())
                if(method.name() == name)
                    return method;

            if(include_bases)
                for(auto &base : bases())
                    if(auto method = base->resolve_method(name); method.has_value())
                        return method;

            return dib::option::none;
        }
        
        inline size_t Type::get_hash() const
        {
            return v->get_hash();
        }
        constexpr bool Type::operator==(const Type &other) const
        {
            return *v == *other.v 
                && _is_const == other._is_const 
                && _is_volatile == other._is_volatile;
        }
    //#endregion
    
    struct Manager 
    {
    private:
        std::vector<Type> _types;
        std::unordered_map<std::string_view, size_t> _by_name;
        std::unordered_map<Type, size_t> _by_type;

    public:
        constexpr void register_type(Type type)
        {
            if(_by_name.contains(type.name())) [[unlikely]]
                RUNTIME_ERROR("Cannot register a type with name {}; a type with this name is already registered!", type.name());

            _by_name.insert({type.name(), _types.size()});
            _by_type.insert({type, _types.size()});
            _types.push_back(type);
        }

        constexpr void unregister_type(Type type)
        {
            if(!_by_type.contains(type)) [[unlikely]]
                RUNTIME_ERROR("Cannot unregister type {}; type not registered!", type.name());

            auto it = _by_type.find(type);
            auto i = it->second;

            _by_type.erase(it);
            _types.erase(_types.begin() + i);
            _by_name.erase(type.name());
        }

        constexpr std::span<const Type> types() const 
        {
            return _types;
        }

        constexpr dib::option::Option<Type> resolve(std::string_view name) const
        {
            auto it = _by_name.find(name);

            if(it == _by_name.end())
                return dib::option::none;

            return _types[it->second];
        }
    };

    // TODO: move to cpp file for external linkage //
    constexpr Manager &reflection_manager()
    {
        static Manager manager;
        return manager;
    }

    namespace detail
    {
        struct Empty {};
    }

    /// Returns a statically defined string view representing the name of some 
    /// reflected object.
    template<std::meta::info I>
    consteval std::string_view name_of()
    {
        if constexpr(std::meta::is_operator_function(I))
        {
            constexpr auto op = std::meta::operator_of(I);
            return std::define_static_string((std::string)"operator" + std::meta::symbol_of(op));
        }
        else if constexpr(std::meta::has_identifier(I))
        {
            return std::define_static_string(std::meta::identifier_of(I));
        }
        else if constexpr(std::meta::is_type(I) || std::meta::is_function(I))
        {
            // todo; replace with a handrolled version of display string of that is deterministic
            return std::define_static_string(std::meta::display_string_of(I));
        }
        else 
        {
            return std::define_static_string("");
        }
    }

    /// Complete a definition by filling out all common information about it.
    /// This includes the definition's name, and its annotations.
    template<std::meta::info I>
    consteval void ReflectionImpl::finalize_definition(Definition &def)
    {
        using namespace std::meta;
        using namespace std::ranges;

        // Collect all attributes into a static array.
        auto attributes = std::vector<AnyRef>{};

        // TODO: re-enable; right now this code throws a fake error via clangd when definition
        // TODO: of annotated type is outside the current file
        #if 0
        constexpr auto annotations = std::define_static_array(std::meta::annotations_of(I));
        
        template for(constexpr auto a : annotations)
        {
            using A = [:std::meta::type_of(a):];
            
            constexpr auto attr_vec = std::meta::extract<A>(a);
            auto &attr_val = std::define_static_array(std::span<const A>(&attr_vec, 1))[0];
            
            attributes.push_back(AnyRef(ReflectionImpl::compute_type<A>(), (void *)&attr_val));
        }
        #endif

        def._attributes = dib::define_any_static_array(attributes);

        // Compute the appropriate name of the definition
        def._name = name_of<I>();

        // Compute if the provided definition is a private or protected member of a class
        if constexpr(!std::meta::is_class_member(I))
        {
            def._is_private = std::meta::is_private(I);
            def._is_protected = std::meta::is_protected(I);
        }
        else
        {
            def._is_private = false;
            def._is_protected = false;
        }
    }
    
    template<class T>
    constexpr TypeStorage type_storage_of = ReflectionImpl::compute_type_storage<std::remove_cv_t<T>>();

    template<class T>
    constexpr Type typeof_exact = ReflectionImpl::compute_type<T>();
    template<class T>
    constexpr Type typeof = typeof_exact<std::remove_cvref_t<T>>;
    
    template<class T>
    const Type *ReflectionImpl::compute_type_ptr()
    {
        return &typeof<T>;
    }
    
    template<class T>
    constexpr Type ReflectionImpl::compute_type()
    {
        Type result;

        result.v = &type_storage_of<T>;
        result._is_const = std::is_const_v<T>;
        result._is_volatile = std::is_volatile_v<T>;
        
        return result;
    }

    template<class T, std::meta::info FieldInfo>
    constexpr Field ReflectionImpl::compute_field()
    {
        using namespace std::meta;
        using namespace std::ranges;

        auto field = Field{};

        constexpr auto fieldty = type_of(FieldInfo);
        using F = typename [:fieldty:];

        field._type = &typeof_exact<F>;

        if constexpr(is_static_member(FieldInfo))
        {
            // We can only get a copy of a field if it has a copy constructor //
            if constexpr(is_copy_constructible_type(fieldty))
            {
                field._get = [](AnyRef) -> Any
                { 
                    return Any::from<F>(static_cast<F &&>([:FieldInfo:])); 
                };
            }
            else
            {
                field._get = nullptr;
            }

            // We can only set a field if it is not a reference, not const, and can be move assigned //
            if constexpr(!is_const(fieldty) && !is_reference_type(fieldty) && is_move_assignable_type(fieldty))
            {
                field._set = [](AnyRef, AnyRef value) 
                { 
                    [:FieldInfo:] = MOVE(value.as<F>()); 
                };
            }
            else 
            {
                field._set = nullptr;
            }

            // We can only get a reference to a field if it is not itself a reference //
            if constexpr(!is_reference_type(fieldty))
            {
                field._ref = [](AnyRef) -> AnyRef
                {
                    return AnyRef::from<F>([:FieldInfo:]);
                };
            }
            else 
            {
                field._ref = nullptr;
            }

            field._is_static = true;
        }
        else
        {
            // We can always get a field //
            if constexpr(is_copy_constructible_type(fieldty))
            {
                field._get = [](AnyRef self) -> Any
                { 
                    return Any::from<F>(self.as<T, ConstBehaviour::ignore>().[:FieldInfo:]); 
                };
            }
            else 
            {
                field._get = nullptr;
            }

            // We can only set a field if it is not a reference and not const //
            if constexpr(!is_const(fieldty) && !is_reference_type(fieldty) && is_move_assignable_type(fieldty))
            {
                field._set = [](AnyRef self, AnyRef value) 
                { 
                    self.as<T, ConstBehaviour::ignore>().[:FieldInfo:] = MOVE(value.as<F>()); 
                };
            }
            else 
            {
                field._set = nullptr;
            }

            // We can only get a reference to a field if it is not itself a reference //
            if constexpr(!is_reference_type(fieldty) && !is_bit_field(FieldInfo))
            {
                field._ref = [](AnyRef self) -> AnyRef
                {
                    return AnyRef::from<F>(self.as<T, ConstBehaviour::ignore>().[:FieldInfo:]);
                };
            }
            else 
            {
                field._ref = nullptr;
            }

            field._is_static = true;
        }

        finalize_definition<FieldInfo>(field);
        return field;
    }
    
    template<class T, std::meta::info MethodInfo>
    constexpr Overload ReflectionImpl::compute_overload()
    {
        using namespace std::meta;
        using namespace std::ranges;

        constexpr auto retty = return_type_of(MethodInfo);
        using RetTy = typename [:retty:];

        // Fill out the overload //
        auto overload = Overload {};

        auto arg_types = std::vector<const Type *>{};
        constexpr auto arg_types_info = std::define_static_array(
            parameters_of(MethodInfo) | views::transform(type_of));

        template for(constexpr auto p : arg_types_info)
        {
            arg_types.push_back(&typeof_exact<typename [:p:]>);
        }

        overload._argument_types = std::define_static_array(arg_types);
        overload._return_type = &typeof_exact<RetTy>;
        overload._is_virtual = is_virtual(MethodInfo);
        overload._is_const = is_const(MethodInfo);
        overload._is_static = is_static_member(MethodInfo);

        // Define the wrapper for the provided function //
        overload._wrapper = [](AnyRef selfref, std::span<AnyRef> args, AnyRef &ret) constexpr
        {
            // Iterate the list of argument types //
            using ArgList = typename [:
                std::meta::substitute(^^dib::meta::List, arg_types_info) :];

            auto unwrap_arg = [&, i=0uz]<class Arg>(dib::meta::Return<Arg>) mutable -> Arg
            {
                return args[i++].as<Arg>();
            };

            // Splat the argument list onto this lambda //
            dib::meta::splat_list<ArgList>([&]<class ...Args>
            {
                // Execute a single call //
                auto call = [&] -> decltype(auto)
                {
                    if constexpr(is_static_member(MethodInfo))
                    {
                        return std::invoke(&[:MethodInfo:], 
                            FORWARD(unwrap_arg(dib::meta::Return<Args>{}))...);
                    }
                    else 
                    {
                        return std::invoke(&[:MethodInfo:], 
                            selfref.as<T, ConstBehaviour::ignore>(), 
                            FORWARD(unwrap_arg(dib::meta::Return<Args>{}))...);
                    }
                };

                // Perform the proper assignment based on the return type's category //
                if constexpr (std::is_void_v<RetTy>)
                {
                    call();
                }
                else if constexpr(std::is_reference_v<RetTy>)
                {
                    ret = AnyRef::from<RetTy>((RetTy)(call()));
                }
                else if constexpr(std::is_move_assignable_v<RetTy>)
                {
                    ret.as<RetTy>() = MOVE(call());
                }
                else 
                {
                    RUNTIME_ERROR(
                        "Return value is of type {}, which is non-move-assignable; functions that return these types cannot be invoked via reflection.",
                        compute_type<RetTy>().name());
                }
            });
        };

        finalize_definition<MethodInfo>(overload);
        return overload;
    }
    
    template<class T>
    consteval TypeStorage ReflectionImpl::compute_type_storage()
    {
        using namespace std::meta;
        using namespace std::ranges;

        constexpr auto C = ^^T;

        TypeStorage result;
        result._rtti = &typeid(T);

        auto fields = std::vector<Field>{};
        auto methods = std::vector<Method>{};
        auto bases = std::vector<const Type *>{};

        // TODO; fix the reflection system up and revert this.
        constexpr auto depth = ReflectionDepth::none;
        /*
            has_annotation(C, ^^OmitFromReflection) ? ReflectionDepth::none :
            has_annotation(C, ^^ReflectAll) ? ReflectionDepth::all :
            ReflectionDepth::public_api;
        */
        constexpr auto access = 
            depth == ReflectionDepth::all ? access_context::unchecked() :
            access_context::unprivileged();


        // Handle class definitions here //
        if constexpr(is_class_type(C) && depth > ReflectionDepth::none)
        {
            // Define a filter for fields //
            constexpr auto is_field = [](auto x) 
            { 
                return is_nonstatic_data_member(x) || is_variable(x); 
            };

            // Fill out the list of fields for the type //
            template for(constexpr auto f : std::define_static_array(
                members_of(C, access) | views::filter(is_field)))
            {
                auto field = ReflectionImpl::compute_field<T, f>();
                fields.push_back(field);
            }

            // Fill out the list of methods //
            std::vector<std::pair<std::string_view, std::vector<Overload>>> overload_map;
            constexpr auto id_C = name_of<has_template_arguments(C) ? template_of(C) : C>();

            template for(constexpr auto m : std::define_static_array(
                members_of(C, access)
                    | views::filter(is_function)
                    | views::filter(std::not_fn(is_operator_function))
                    | views::filter(std::not_fn(is_function_template))
                    | views::filter(std::not_fn(is_constructor))
                    | views::filter(std::not_fn(is_destructor))
                    | views::filter(std::not_fn(is_rvalue_reference_qualified))))
                    // TODO: the above condition ^^^^^^^^^^^^^^^^^^^^^^^^^^^ should be supported,
                    // but I am too lazy to do it right now
            {
                constexpr auto id_m = name_of<m>();

                if constexpr(!is_unsafe_for_reflection<strings::make_const<id_C.data()>(), strings::make_const<id_m.data()>()>())
                {
                    // Find the method associated with the provided name //
                    std::string_view name = id_m;
                    std::vector<Overload> *method = nullptr;

                    for(auto i = 0uz; i < overload_map.size(); i++)
                    {
                        if(overload_map[i].first == name)
                        {
                            method = &overload_map[i].second;
                            break;
                        }
                    }
                    
                    if(method == nullptr)
                    {
                        overload_map.push_back({name, {}});
                        method = &overload_map[overload_map.size() - 1].second;
                    }

                    auto overload = ReflectionImpl::compute_overload<T, m>();
                    method->push_back(overload);
                }
            }

            for(auto [name, overloads] : overload_map)
            {
                auto method = Method{};

                method._name = name;
                method._overloads = dib::define_any_static_array(overloads);

                methods.push_back(method);
            }

            // Fill out the list of base classes //
            template for(constexpr auto base : std::define_static_array(bases_of(
                C, access_context::unchecked())))
            {
                bases.push_back(&typeof_exact<typename [: type_of(base) :]>);
            }
        }

        result._fields = dib::define_any_static_array(fields);
        result._methods = dib::define_any_static_array(methods);
        result._bases = dib::define_any_static_array(bases);

        // Fill in the static type information //
        constexpr auto isref = std::is_reference_v<T>;
        constexpr auto isfn = std::is_function_v<T>;
        constexpr auto isvoid = std::is_void_v<T>;
        constexpr auto isarray = std::is_array_v<T>;

        constexpr auto has_ops = !isref && !isfn && !isvoid & !isarray;

        if constexpr (std::is_trivially_destructible_v<T> || !has_ops)
        {
            result._destruct = [](void *) {};
        }
        else
        {
            result._destruct = [](void *value) { reinterpret_cast<T *>(value)->~T(); };
        }

        if constexpr (has_ops)
        {
            result._delete = [](void *dst) { return delete (T*)dst; };
            result._delete_array = [](void *dst) { return delete[] (T*)dst; };
        }

        if constexpr (std::is_default_constructible_v<T> && has_ops)
        {
            result._default_construct = [](void *src) { std::construct_at<T>((T *)src); };
            result._new = [] { return (void*)new T(); };
            result._new_array = [](size_t count) { return (void*)new T[count]; };
        }

        if constexpr (std::is_move_constructible_v<T> && has_ops)
        {
            result._move_construct = [](void *src, void *dest)
            {
                new(dest) T(reinterpret_cast<T &&>(*reinterpret_cast<T *>(src)));
            };
        }

        if constexpr (std::is_copy_constructible_v<T> && has_ops)
        {
            result._copy_construct = [](void *src, void *dest)
            {
                new(dest) T(reinterpret_cast<T &>(*reinterpret_cast<T *>(src)));
            };
        }

        if constexpr (types::IsRelocatable<T> && has_ops)
        {
            result._relocate = [](void *src, void *dest)
            {
                dib::uninitialized_relocate((T *)src, (T *)dest);
            };
        }

        if constexpr (std::is_swappable_v<T> && has_ops)
        {
            result._swap = [](void *src, void *dest)
            {
                std::swap(*(T *)src, *(T *)dest);
            };
        }

        if constexpr (types::IsHashable<T> && has_ops)
        {
            result._hash = [](void *src)
            {
                return std::hash<T>{}(*(T*)src);
            };
        }

        if constexpr (types::IsEqualityComparable<T> && has_ops)
        {
            result._eq = [](void *lhs, void *rhs)
            {
                return *((const T *)lhs) == *((const T *)rhs);
            };
        }

        if constexpr (types::IsThreeWayComparable<T> && has_ops)
        {
            result._less = [](void *lhs, void *rhs)
            {
                return *((const T *)lhs) < *((const T *)rhs);
            };
        }

        result._size = dib::types::sizeof_<T>;
        result._packed_size = dib::types::packed_sizeof<T>;
        result._align = dib::types::alignof_<T>;

        result._polymorphic = std::is_polymorphic_v<T>;
        result._trivial = std::is_trivially_default_constructible_v<T> && std::is_trivially_copyable_v<T>;
        result._trivial_copy = std::is_trivially_copyable_v<T>;
        result._trivial_dest = std::is_trivially_destructible_v<T>;
        result._trivial_move = std::is_trivially_move_constructible_v<T>;
        result._trivial_rel = dib::types::is_trivially_relocatable<T>;

        result._is_pointer = std::is_pointer_v<T>;
        result._is_lv_reference = std::is_lvalue_reference_v<T>;
        result._is_rv_reference = std::is_rvalue_reference_v<T>;

        if constexpr(std::is_pointer_v<T>)
        {
            result._pointee = &typeof_exact<std::remove_pointer_t<T>>;
        }
        else if constexpr(std::is_reference_v<T>)
        {
            result._pointee = &typeof_exact<std::remove_reference_t<T>>;
        }

        if constexpr(std::is_function_v<T>)
        {
            result._return = &typeof_exact<dib::meta::FunctionGetReturn::Call<T>>;
            
            using Args = dib::meta::FunctionGetArguments::Call<T>;
            auto args = std::vector<const Type *>{};

            template for(constexpr auto arg : Args::types)
            {
                using Arg = [:arg:];
                args.push_back(&typeof_exact<Arg>);
            }

            result._arguments = dib::define_any_static_array(args);
        }
        else
        {
            result._arguments = {};
        }

        finalize_definition<C>(result);

        return result;
    }
    

    consteval
    {
        mark_unsafe_for_reflection<"tuple", "swap">();
    }
}