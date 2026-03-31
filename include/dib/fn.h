#pragma once

namespace dib::functional
{
    /// Helper alias for function pointers
    namespace detail
    {
        template<class>
        struct FnPtrHelper
        {};

        template<class R, class... A>
        struct FnPtrHelper<R(A...)>
        {
            using type = R(*)(A...);
        };
    }
    
    
    template<class T>
    using fn = typename detail::FnPtrHelper<T>::type;
}