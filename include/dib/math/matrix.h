#pragma once

#include "externs.h"
#include "raylib.h"
#include "raymath.h"
#include "vec.h"

#include "dib/preprocess.h"

namespace dib::math
{
    constexpr struct RowMajorType {} row_major;

    template<class T, size_t C, size_t R>
    class matrix;

    namespace detail
    {
        template<class T, size_t C, size_t R>
        class matrix_base
        {
        protected:
            std::array<T, C*R> _array;

        public:
            constexpr explicit matrix_base(T value = (T)0)
            {
                _array.fill(value);
            }

            constexpr explicit matrix_base(std::array<T, C*R> &&array)
                : _array(MOVE(array))
            {}
            
            constexpr explicit matrix_base(RowMajorType, std::array<T, C*R> &&array)
            {
                for(size_t i = 0; i < R; i++)
                    for(size_t j = 0; j < C; j++)
                        (*this)[i, j] = array[i * C + j];
            }

            constexpr matrix_base(vec<T, R> vec) requires (C == 1)
            {
                for(size_t i = 0; i < R; i++)
                    _array[i] = vec[i];
            }
            
            template<class X>
            constexpr explicit operator matrix<X, R, C>() const 
            {
                matrix<X, R, C> o;

                for(size_t i = 0; i < R; i++)
                    for(size_t j = 0; j < C; j++)
                        o[i,j] = static_cast<X>((*this)[i,j]);
                
                return o;
            }

            constexpr operator vec<T, R>() const requires (C == 1)
            {
                vec<T, R> o;

                for(size_t i = 0; i < R; i++)
                    o[i] = _array[i];

                return o;
            }
            
            constexpr operator vec<T, C>() const requires (R == 1)
            {
                vec<T, C> o;
                
                for(size_t i = 0; i < C; i++)
                    o[i] = _array[i];

                return o;
            }
            
            constexpr std::array<T, R*C> &as_array() {return _array;}
            constexpr const std::array<T, R*C> &as_array() const {return _array;}

            constexpr T &operator[](size_t r, size_t c) 
            {
                if(r >= R || c >= C)
                    RUNTIME_ERROR("Index ({}, {}) out of bounds for matrix of dimension {}x{}", r, c, C, R);

                return _array[r + (c * R)];
            }

            constexpr const T &operator[](size_t r, size_t c) const
            {
                return const_cast<matrix_base<T, C, R>*>(this)->operator[](r, c);
            }

            constexpr vec<T, C> get_row(size_t r) const
            {
                vec<T, C> value;

                for(size_t i = 0; i < C; i++)
                    value[i] = (*this)[r, i];

                return value;
            }
            
            constexpr vec<T, R> get_col(size_t c) const
            {
                vec<T, R> value;

                for(size_t i = 0; i < R; i++)
                    value[i] = (*this)[i, c];

                return value;
            }
            
            const static matrix<T, C, R> identity;

            matrix_base(const Matrix &value) 
                requires (R == C && C == 4 && std::is_same_v<T, float>)
            {
                auto varr = MatrixToFloatV(value);
                for(size_t i = 0; i < 16; i++)
                    _array[i] = varr.v[i];
            }

            operator Matrix() const 
                requires (R == C && C == 4 && std::is_same_v<T, float>)
            {
                Matrix o;

                o.m0 = (*this)[0, 0];
                o.m1 = (*this)[0, 1];
                o.m2 = (*this)[0, 2];
                o.m3 = (*this)[0, 3];

                o.m4 = (*this)[1, 0];
                o.m5 = (*this)[1, 1];
                o.m6 = (*this)[1, 2];
                o.m7 = (*this)[1, 3];
                
                o.m8 = (*this)[2, 0];
                o.m9 = (*this)[2, 1];
                o.m10 = (*this)[2, 2];
                o.m11 = (*this)[2, 3];
                
                o.m12 = (*this)[3, 0];
                o.m13 = (*this)[3, 1];
                o.m14 = (*this)[3, 2];
                o.m15 = (*this)[3, 3];

                return o;
            }
        };
    }

    template<class T, size_t C, size_t R>
    class matrix : public detail::matrix_base<T, C, R>
    {
        using detail::matrix_base<T, C, R>::matrix_base;
    };

    template<class T, size_t N>
    matrix(vec<T, N>) -> matrix<T, 1, N>;

    template<class T, size_t C, size_t R> const matrix<T, C, R> detail::matrix_base<T, C, R>::identity = []
        {
            matrix<T, C, R> o;

            for(size_t i = 0; i < C && i < R; i++)
                o[i, i] = (T)1;

            return o;
        }
    ();

    #define __func(T, N, M) \
        using T##N##x##M = matrix<T, N, M>;

    __DIBMATH_ALL_EXTERNS(2)
    __DIBMATH_ALL_EXTERNS(3)
    __DIBMATH_ALL_EXTERNS(4)
    #undef __func

    template<class T, size_t R1, size_t H, size_t C2>
    constexpr matrix<T, C2, R1> operator*(const matrix<T, H, R1> &l, const matrix<T, C2, H> &r)
    {
        matrix<T, C2, R1> o;

        for(size_t i = 0; i < R1; i++)
            for(size_t j = 0; j < C2; j++)
                o[i, j] = dot(l.get_row(i), r.get_col(j));

        return o;
    }

    template<class T, size_t C, size_t R>
    constexpr vec<T, C> operator*(const matrix<T, C, R> &l, vec<T, C> r)
    {
        return l * matrix(r);
    }

    template<class T, size_t C, size_t R>
    constexpr matrix<T, R, C> transpose(const matrix<T, C, R> &m)
    {
        matrix<T, R, C> o;

        for(size_t i = 0; i < R; i++)
            for(size_t j = 0; j < C; j++)
                o[j, i] = m[i, j];

        return o;
    }
}