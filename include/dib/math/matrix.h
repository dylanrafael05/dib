#ifndef __DIBMATH_MATRIX_H
#define __DIBMATH_MATRIX_H

#include "externs.h"
#include "vec.h"
#include <bit>

#ifndef __DIBMATH_MATRIX_IMPL
static_assert(false, "math/matrix.h is unfinished.");
#endif

namespace dib::math
{
    template<class T, size_t R, size_t C>
    class matrix;

    namespace detail
    {
        template<class T, size_t R, size_t C>
        class matrix_base;
        
        template<class T, size_t C>
        class matrix_base<T, 2, C>
        {
        public:
            vec<T, C> x;
            vec<T, C> y;

            constexpr matrix_base() {}
            constexpr matrix_base(vec<T, C> x, vec<T, C> y)
                : x(x), y(y)
            {}
        };
        
        template<class T, size_t C>
        class matrix_base<T, 3, C> : public matrix_base<T, 2, C>
        {
        public:
            vec<T, C> z;

            constexpr matrix_base() {}
            constexpr matrix_base(vec<T, C> x, vec<T, C> y, vec<T, C> z)
                : matrix_base<T, 2, C>(x, y), z(z)
            {}
        };
        
        template<class T, size_t C>
        class matrix_base<T, 4, C> : public matrix_base<T, 3, C>
        {
        public:
            vec<T, C> w;

            constexpr matrix_base() {}
            constexpr matrix_base(vec<T, C> x, vec<T, C> y, vec<T, C> z, vec<T, C> w)
                : matrix_base<T, 3, C>(x, y, z), w(w)
            {}
            
            operator Matrix() const requires std::same_as<T, float>
            {
                Matrix o;

                o.m0 = (*this)[0][0];
                o.m1 = (*this)[0][1];
                o.m2 = (*this)[0][2];
                o.m3 = (*this)[0][3];

                o.m4 = (*this)[1][0];
                o.m5 = (*this)[1][1];
                o.m6 = (*this)[1][2];
                o.m7 = (*this)[1][3];
                
                o.m8 = (*this)[2][0];
                o.m9 = (*this)[2][1];
                o.m10 = (*this)[2][2];
                o.m11 = (*this)[2][3];
                
                o.m12 = (*this)[3][0];
                o.m13 = (*this)[3][1];
                o.m14 = (*this)[3][2];
                o.m15 = (*this)[3][3];

                return o;
            }
        };
        
        template<class T, size_t R, size_t C>
        class matrix_base : public matrix_base<T, 4, C>
        {
        public:
            std::array<vec<T, C>, R-4> rest;

            constexpr matrix_base() {}
            constexpr matrix_base(vec<T, C> x, vec<T, C> y, vec<T, C> z, vec<T, C> w, std::array<vec<T, C>, R-4> rest)
                : matrix_base<T, 4, C>(x, y, z, w), rest(rest)
            {}
        };

        template<class T, size_t R, size_t C>
        struct matrix_consts {};

        template<class T, size_t N>
        struct matrix_consts<T, N, N> 
        {
            static const matrix<T, N, N> identity;
        };
    }

    template<class T, size_t R, size_t C>
    class matrix : public detail::matrix_base<T, R, C>, public detail::matrix_consts<T, R, C>
    {
        // ARRAY ACCESSORS //
        std::array<T, R*C> &as_array() {return *reinterpret_cast<std::array<T, R*C>*>(this);}
        const std::array<T, R*C> &as_array() const {return *reinterpret_cast<const std::array<T, R*C>*>(this);}
        
        std::array<vec<T, C>, R> &as_row_array() {return *reinterpret_cast<std::array<vec<T, C>, R>*>(this);}
        const std::array<vec<T, C>, R> &as_row_array() const {return *reinterpret_cast<const std::array<vec<T, C>, R>*>(this);}

        vec<T, C> &operator[](size_t x) {return as_row_array()[x];}
        const vec<T, C> &operator[](size_t x) const {return as_row_array()[x];}
        
        // CONSTRUCTORS //
        template<class X>
        constexpr explicit operator matrix<X, R, C>() const 
        {
            matrix<X, R, C> out;
            for(size_t i = 0; i < R; i++)
                for(size_t j = 0; j < C; j++)
                    out[i][j] = static_cast<X>((*this)[i][j]);
            return out;
        }

        using detail::matrix_base<T, R, C>::matrix_base;
    };

    template<class T, size_t N> constexpr matrix<T, N, N> detail::matrix_consts<T, N, N>::identity = []
        {
            std::array<T, N*N> out{(T)0};

            for(size_t i = 0; i < N; i++)
                out[i + i*N] = (T)1;
            
            return std::bit_cast<matrix<T, N, N>>(out);
        }
    ();

    // TODO: why does this return a non-identity matrix?
    template<class T, size_t N> constexpr auto mat_identity = []
        {
            std::array<T, N*N> out{(T)0};

            for(size_t i = 0; i < N; i++)
                out[i + i*N] = (T)1;
            
            return std::bit_cast<matrix<T, N, N>>(out);
        }
    ();

    #define __func(T, N, M) \
        using T##N##x##M = matrix<T, N, M>;

    __DIBMATH_ALL_EXTERNS(2)
    __DIBMATH_ALL_EXTERNS(3)
    __DIBMATH_ALL_EXTERNS(4)
    #undef __func


}

constexpr auto M = dib::math::mat_identity<float, 4>;

#endif