#ifndef __DIB_MATH_VEC_H
#define __DIB_MATH_VEC_H

#include <stddef.h>
#include <array>
#include <cmath>

#include "concept.h"
#include "externs.h"

#include "raylib.h"
#include "dib/types.h"

namespace dib::math
{
    using uint = unsigned int;

    template<class T, size_t N>
    class vec;

    namespace detail
    {
        template<class T, size_t N>
        class vec_base;

        template<class T>
        class vec_base<T, 2>
        {
        public:
            T x = (T)0;
            T y = (T)0;

            constexpr vec_base(T x, T y)
                : x(x), y(y)
            {}
            constexpr vec_base(T e)
                : x(e), y(e)
            {}
            
            constexpr vec_base()
            {}
            
            operator Vector2() const requires std::same_as<T, float>
            {
                return *reinterpret_cast<const Vector2*>(this);
            }
        };
        
        template<class T>
        class vec_base<T, 3> : public vec_base<T, 2>
        {
        public:
            T z = (T)0;
            
            using vec_base<T, 2>::vec_base;
            
            constexpr vec_base(T x, T y, T z)
                : vec_base<T, 2>(x, y), z(z)
            {}
            constexpr vec_base(T e)
                : vec_base<T, 2>(e), z(e)
            {}
            
            constexpr vec_base()
            {}
            
            operator Vector3() const requires std::same_as<T, float>
            {
                return *reinterpret_cast<const Vector3*>(this);
            }
        };
        
        template<class T>
        class vec_base<T, 4> : public vec_base<T, 3>
        {
        public:
            T w = (T)0;
            
            using vec_base<T, 3>::vec_base;
            
            constexpr vec_base(T x, T y, T z, T w)
                : vec_base<T, 3>(x, y, z), w(w)
            {}
            constexpr vec_base(T e)
                : vec_base<T, 3>(e), w(e)
            {}
            
            constexpr vec_base()
            {}

            operator Vector4() const requires std::same_as<T, float>
            {
                return *reinterpret_cast<const Vector4*>(this);
            }
        };
        
        template<class T, size_t N>
        class vec_base : public vec_base<T, 4>
        {
        public:
            std::array<T, N - 4> rest = { (T)0 };
            
            using vec_base<T, 4>::vec_base;

            constexpr vec_base(T x, T y, T z, T w, std::array<T, N - 4> rest)
                : vec_base<T, 4>(x, y, z, w), rest(rest)
            {}
            constexpr vec_base(T e)
                : vec_base<T, 4>(e), rest(e)
            {}
            
            constexpr vec_base()
            {}
        };
        
        template<class T, size_t N, size_t Real>
        struct vec_consts;

        template<class T, size_t Real>
        struct vec_consts<T, 2, Real>
        {
            static const vec<T, Real> left;
            static const vec<T, Real> right;
            static const vec<T, Real> up;
            static const vec<T, Real> down;
        };

        template<class T, size_t Real>
        struct vec_consts<T, 3, Real> : public vec_consts<T, 2, Real>
        {
            static const vec<T, Real> fwd;
            static const vec<T, Real> bwd;
        };
        
        template<class T, size_t N, size_t Real>
        struct vec_consts : public vec_consts<T, 3, Real>
        {};
    }

    template<class T, size_t N>
    class vec : public detail::vec_base<T, N>, public detail::vec_consts<T, N, N>, types::TriviallyRelocatable
    {
    public:
        // ARRAY ACCESSORS //
        std::array<T, N> &as_array() {return *reinterpret_cast<std::array<T, N>*>(this);}
        const std::array<T, N> &as_array() const {return *reinterpret_cast<const std::array<T, N>*>(this);}

        T &operator[](size_t x) {return as_array()[x];}
        const T &operator[](size_t x) const {return as_array()[x];}

        // CONSTRUCTORS //
        template<class X>
        constexpr explicit operator vec<X, N>() const 
        {
            vec<X, N> out;
            for(size_t i = 0; i < N; i++)
                out[i] = static_cast<X>((*this)[i]);
            return out;
        }

        using detail::vec_base<T, N>::vec_base;

        // TUPLE PROTOCOL //
        template<size_t I> T &get()
        {
            return (*this)[I];
        }
        
        template<size_t I> const T &get() const
        {
            return (*this)[I];
        }
    };

    // Constants //
    template<class T, size_t N> const vec<T, N> detail::vec_consts<T, 2, N>::right = {+1, 0};
    template<class T, size_t N> const vec<T, N> detail::vec_consts<T, 2, N>::left  = {-1, 0};
    template<class T, size_t N> const vec<T, N> detail::vec_consts<T, 2, N>::up    = {0, +1};
    template<class T, size_t N> const vec<T, N> detail::vec_consts<T, 2, N>::down  = {0, -1};
    
    template<class T, size_t N> const vec<T, N> detail::vec_consts<T, 3, N>::fwd = {0, 0, +1};
    template<class T, size_t N> const vec<T, N> detail::vec_consts<T, 3, N>::bwd = {0, 0, -1};

    // Standard operators //
    #define __func(T, N, op)                                                     \
        extern template vec<T, N> operator op <T, N>(vec<T, N>, vec<T, N>);      \
        extern template vec<T, N> operator op <T, N>(vec<T, N>, T);              \
        extern template vec<T, N> operator op <T, N>(T, vec<T, N>);              \
        extern template vec<T, N> &operator op##= <T, N>(vec<T, N>&, vec<T, N>); \
        extern template vec<T, N> &operator op##= <T, N>(vec<T, N>&, T);
    
    #define impl_op(op, set, ...)                                            \
        template<class T, size_t N>                                          \
        vec<T, N> operator op(vec<T, N> lhs, vec<T, N> rhs)                  \
            requires requires(T x, T y) {x op y;}                            \
        {                                                                    \
            vec<T, N> out;                                                   \
            for(size_t i = 0; i < N; i++)                                    \
                out[i] = lhs[i] op rhs[i];                                   \
            return out;                                                      \
        }                                                                    \
                                                                             \
        template<class T, size_t N>                                          \
        vec<T, N> operator op(vec<T, N> lhs, T rhs)                          \
            requires requires(T x, T y) {x op y;}                            \
        {                                                                    \
            vec<T, N> out;                                                   \
            for(size_t i = 0; i < N; i++)                                    \
                out[i] = lhs[i] op rhs;                                      \
            return out;                                                      \
        }                                                                    \
        template<class T, size_t N>                                          \
        vec<T, N> operator op(T lhs, vec<T, N> rhs)                          \
            requires requires(T x, T y) {x op y;}                            \
        {                                                                    \
            vec<T, N> out;                                                   \
            for(size_t i = 0; i < N; i++)                                    \
                out[i] = lhs op rhs[i];                                      \
            return out;                                                      \
        }                                                                    \
                                                                             \
        template<class T, size_t N>                                          \
        vec<T, N> &operator op##=(vec<T, N> &lhs, vec<T, N> rhs)             \
            requires requires(T &x, T y) {x op##= y;}                        \
        {                                                                    \
            for(size_t i = 0; i < N; i++)                                    \
                lhs[i] op##= rhs[i];                                         \
            return lhs;                                                      \
        }                                                                    \
                                                                             \
        template<class T, size_t N>                                          \
        vec<T, N> &operator op##=(vec<T, N> &lhs, T rhs)                     \
            requires requires(T &x, T y) {x op##= y;}                        \
        {                                                                    \
            for(size_t i = 0; i < N; i++)                                    \
                lhs[i] op##= rhs;                                            \
            return lhs;                                                      \
        }                                                                    \
                                                                             \
        set(op)

    impl_op(+, __DIBMATH_SCALAR_EXTERNS) 
    impl_op(-, __DIBMATH_SCALAR_EXTERNS) 
    impl_op(*, __DIBMATH_SCALAR_EXTERNS) 
    impl_op(/, __DIBMATH_SCALAR_EXTERNS) 
    impl_op(%, __DIBMATH_INTEGRAL_EXTERNS)
    impl_op(&, __DIBMATH_BITWISE_EXTERNS)
    impl_op(|, __DIBMATH_BITWISE_EXTERNS)
    impl_op(^, __DIBMATH_BITWISE_EXTERNS)

    #undef impl_op
    #undef __func

    #define __func(T, N, op) \
        extern template vec<T, N> operator op(vec<T, N>);
    
    #define impl_op(op, set, ...)            \
        template<class T, size_t N>          \
        vec<T, N> operator op(vec<T, N> val) \
            requires requires(T x) {op x;}   \
        {                                    \
            vec<T, N> out;                   \
            for(size_t i = 0; i < N; i++)    \
                out[i] = op val[i];          \
            return out;                      \
        }                                    \
                                             \
        set(op)

    impl_op(+, __DIBMATH_SCALAR_EXTERNS)
    impl_op(-, __DIBMATH_SCALAR_EXTERNS)
    impl_op(!, __DIBMATH_BOOL_EXTERNS)
    impl_op(~, __DIBMATH_INTEGRAL_EXTERNS)

    #undef impl_op
    #undef __func

    // Extern class declarations //
    #define __func(T, N, ...) extern template class vec<T, N>;
        __DIBMATH_ALL_EXTERNS()
    #undef __func

    // Using declarations //
    #define __func(T, N, ...) using T##N = vec<T, N>;
        __DIBMATH_ALL_EXTERNS()
    #undef __func

    // Comparison //
    template<class T, size_t N>
    bool operator==(vec<T, N> lhs, vec<T, N> rhs)
    {
        for(size_t i = 0; i < N; i++)
            if(lhs[i] != rhs[i]) return false;

        return true;
    }

    template<class T, size_t N>
    bool operator!=(vec<T, N> lhs, vec<T, N> rhs)
    {
        for(size_t i = 0; i < N; i++)
            if(lhs[i] != rhs[i]) return true;

        return false;
    }

    #define __func(T, N, ...) \
        extern template bool operator==(vec<T, N>, vec<T, N>); \
        extern template bool operator!=(vec<T, N>, vec<T, N>);
    
    __DIBMATH_ALL_EXTERNS()
    #undef __func

    // Vector operations //
    template<concepts::arithmetic T, size_t N>
    T dot(vec<T, N> lhs, vec<T, N> rhs)
    {
        T out = 0;

        for(size_t i = 0; i < N; i++)
            out += lhs[i] * rhs[i];
        
        return out;
    }

    template<concepts::arithmetic T, size_t N>
    float length_sq(vec<T, N> value)
    {
        float sum = 0;

        for(size_t i = 0; i < N; i++)
            sum += static_cast<float>(value[i] * value[i]);

        return sum;
    }
    
    template<concepts::arithmetic T, size_t N>
    float length(vec<T, N> value)
    {
        return sqrtf(length_sq(value));
    }

    #define __func(T, N, ...) \
        extern template T dot(vec<T, N>, vec<T, N>); \
        extern template float length_sq(vec<T, N>); \
        extern template float length(vec<T, N>);
    
    __DIBMATH_SCALAR_EXTERNS()
    #undef __func

    // Normalization //
    template<concepts::real T, size_t N>
    void normalize_inplace(vec<T, N> &value)
    {
        float length = dib::math::length(value);

        for(size_t i = 0; i < N; i++)
            value[i] = static_cast<T>(value[i] / length);
    }

    template<concepts::real T, size_t N>
    vec<T, N> normalize(vec<T, N> value)
    {
        vec<T, N> copy = value;
        normalize_inplace(copy);
        return copy;
    }
    
    #define __func(T, N, ...) \
        extern template vec<T, N> normalize(vec<T, N>); \
        extern template void normalize_inplace(vec<T, N>&);

    __DIBMATH_REAL_EXTERNS()
    #undef __func

    // Max, min, and clamp //
    template<concepts::arithmetic T, size_t N>
    vec<T, N> max(vec<T, N> lhs, vec<T, N> rhs)
    {
        vec<T, N> out;

        for(size_t i = 0; i < N; i++)
            out[i] = lhs[i] > rhs[i] ? lhs[i] : rhs[i];

        return out;
    }
    
    template<concepts::arithmetic T, size_t N>
    vec<T, N> min(vec<T, N> lhs, vec<T, N> rhs)
    {
        vec<T, N> out;

        for(size_t i = 0; i < N; i++)
            out[i] = lhs[i] < rhs[i] ? lhs[i] : rhs[i];

        return out;
    }
    
    template<concepts::arithmetic T, size_t N>
    vec<T, N> clamp(vec<T, N> val, vec<T, N> min, vec<T, N> max)
    {
        vec<T, N> out;

        for(size_t i = 0; i < N; i++)
            out[i] = (val[i] < min[i] ? min[i] : (val[i] > max[i] ? max[i] : val[i]));

        return out;
    }
    
    #define __func(T, N, ...) \
        extern template vec<T, N> max(vec<T, N>, vec<T, N>); \
        extern template vec<T, N> min(vec<T, N>, vec<T, N>); \
        extern template vec<T, N> clamp(vec<T, N>, vec<T, N>, vec<T, N>);

    __DIBMATH_SCALAR_EXTERNS()
    #undef __func

    // Lerp //
    template<concepts::real T, size_t N>
    vec<T, N> lerp(vec<T, N> start, vec<T, N> end, float t)
    {
        if(t == 0) return start;
        if(t == 1) return end;

        vec<T, N> out;

        for(size_t i = 0; i < N; i++)
            out[i] = start[i] + (end[i] - start[i]) * t;

        return out;
    }

    template<concepts::real T, size_t N>
    vec<T, N> clerp(vec<T, N> start, vec<T, N> end, vec<float, N> t)
    {
        vec<T, N> out;

        for (size_t i = 0; i < N; i++)
            out[i] = start[i] + (end[i] - start[i]) * t[i];

        return out;
    }

    #define __func(T, N, ...) extern template vec<T, N> lerp(vec<T, N>, vec<T, N>, float);
        __DIBMATH_REAL_EXTERNS()
    #undef __func

    // Component-wise math //
    template<class T, size_t N, class Lambda>
    constexpr auto cwise(vec<T, N> val, Lambda &&op)
        requires requires {op(val[0]);}
    {
        vec<decltype(op(val[0])), N> out;

        for(size_t i = 0; i < N; i++)
            out[i] = op(val[i]);
        
        return out;
    }
    
    template<class T, size_t N, class Lambda>
    constexpr auto cwise(vec<T, N> lhs, vec<T, N> rhs, Lambda &&op)
        requires requires {op(lhs[0], rhs[0]);}
    {
        vec<decltype(op(lhs[0], rhs[0])), N> out;

        for(size_t i = 0; i < N; i++)
            out[i] = op(lhs[i], rhs[i]);
        
        return out;
    }

    template<class T, size_t N, class Lambda>
    constexpr auto cwise(vec<T, N> v1, vec<T, N> v2, vec<T, N> v3, Lambda &&op)
        requires requires {op(v1[0], v2[0], v3[0]);}
    {
        vec<decltype(op(v1[0], v2[0], v3[0])), N> out;

        for(size_t i = 0; i < N; i++)
            out[i] = op(v1[i], v2[i], v3[i]);
        
        return out;
    }
}

namespace std
{
    template<class T, size_t N> struct tuple_size<dib::math::vec<T, N>> {constexpr static size_t value = N;};
    template<class T, size_t N, size_t I> struct tuple_element<I, dib::math::vec<T, N>> {using type = T;};
}

#endif