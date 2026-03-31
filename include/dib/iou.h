#pragma once

namespace dib
{
    /// A wrapper around an instance of the provided type which can
    /// be used to thwart the static object initialization fiasco,
    /// since construction is performed on first use.
    template<class T>
    class IOU
    {
        union
        {
            T _value;
            char _dummy;
        };

        bool _initialized = false;

    public:
        constexpr IOU()
            : _dummy(0), _initialized(false)
        {}

        constexpr ~IOU()
        {
            if(_initialized) [[likely]]
            {
                _value.~T();
            }
        }

        constexpr T &value() 
        {
            if(!_initialized) [[unlikely]]
            {
                new(&_dummy) T;
                _initialized = true;
            }

            return _value;
        }

        constexpr const T &value() const
        {
            return const_cast<IOU<T>*>(this)->value();
        }
    };
};