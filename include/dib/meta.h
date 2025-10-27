#ifndef __DIBAPP_META_H
#define __DIBAPP_META_H

#include <concepts>

namespace dib::meta
{
    template<class LHS, class RHS>
    concept value_same_as = std::same_as<std::remove_cvref_t<LHS>, std::remove_cvref_t<RHS>>;
    
    template<class LHS, class RHS>
    concept differs_from = !std::same_as<LHS, RHS>;
    
    template<class LHS, class RHS>
    concept value_differs_from = !std::same_as<std::remove_cvref_t<LHS>, std::remove_cvref_t<RHS>>;
}

#endif