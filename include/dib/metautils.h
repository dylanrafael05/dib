#pragma once

#include <algorithm>
#include <meta>
#include <ranges>
#include <vector>

#include "dib/preprocess.h"

namespace dib
{
    namespace detail::blueprints
    {
        /// The core class responsible for handling the 'literalization'
        /// of data; represents all the steps needed to transform any instance of
        /// the provided data type into a desired result.
        struct Blueprint
        {
            struct Assign
            {
                std::meta::info field;
                std::meta::info value;
            };

            const Assign *assignments;
            size_t count;
            bool reconstruct_before_assignments;
            
            std::meta::info type;
        };

        /// A wrapper around std::is_within_lifetime which calls directly to
        /// clang's builtin to circumvent lack of std-library support
        template<class T>
        consteval bool is_within_lifetime(const T *ptr)
        {
        #ifdef DIBCOMPILER_clang
            return __builtin_is_within_lifetime(ptr);
        #else
            return std::is_within_lifetime(ptr);
        #endif
        }
        
        /// A wrapper function which returns a copy of the provided
        /// subobject of some arbitrary value.
        template<std::meta::info Subobject, class T>
        consteval auto access_subobject_of(T value)
        {
            if constexpr(std::meta::is_base(Subobject))
            {
                return *(typename [:std::meta::type_of(Subobject):] *)(&value);
            }
            else
            {
                return value.[:Subobject:];
            }
        }
        
        /// A wrapper function which returns a refernece to the provided
        /// subobject of some arbitrary value.
        template<std::meta::info Subobject, class T>
        consteval auto &refer_to_subobject_of(T &value)
        {
            if constexpr(std::meta::is_base(Subobject))
            {
                return *(typename [:std::meta::type_of(Subobject):] *)(&value);
            }
            else
            {
                return value.[:Subobject:];
            }
        }
        
        /// A wrapper function which overwrites the provided subobject.
        template<std::meta::info Subobject, class T>
        consteval void assign_subobject_of(T &value, auto to)
        {
            if constexpr(std::meta::is_base(Subobject))
            {
                *(typename [:std::meta::type_of(Subobject):] *)(&value) = to;
            }
            else
            {
                value.[:Subobject:] = to;
            }
        }
        
        /// The core blueprint execution system. This is what applies all the
        /// assignments to a type in order to transform it to the desired state.
        template<Blueprint Value, class T>
        consteval void execute_blueprint(T &object);

        /// Perform a single assignment into the provided object.
        template<Blueprint::Assign Value, class T>
        consteval void execute_assignment(T &object)
        {
            // If the assignment is nested, delegate to execute_blueprint for compiletime recursion!
            if constexpr(std::meta::remove_cvref(std::meta::type_of(Value.value)) == ^^Blueprint)
            {
                execute_blueprint<([:Value.value:])>(refer_to_subobject_of<Value.field>(object));
            }
            else
            {
                assign_subobject_of<Value.field>(object, [:Value.value:]);
            }
        }

        // Definition
        template<Blueprint Value, class T>
        consteval void execute_blueprint(T &object)
        {
            if constexpr(Value.reconstruct_before_assignments)
            {
                if(is_within_lifetime(&object))
                    std::destroy_at(&object);
                
                std::construct_at(&object);
            }

            template for(constexpr size_t i : std::ranges::views::iota(0zu, Value.count))
            {
                execute_assignment<Value.assignments[i]>(object);
            }
        }

        /// A helper to get all the nonstatic data members that can be accessed
        /// through splicing by the provided type. Note that these are different
        /// from the nonstatic data members of that type, since anonymous unions
        /// and structs are treated as 'passthrough'.
        template<std::meta::info T>
        consteval std::span<const std::meta::info> nsdms_accessible_from()
        {
            std::vector<std::meta::info> results;

            template for(constexpr auto nsdm : std::define_static_array(
                std::meta::nonstatic_data_members_of(T, std::meta::access_context::unchecked())))
            {
                constexpr auto nsdm_t = std::meta::type_of(nsdm);

                if constexpr(!std::meta::has_identifier(nsdm))
                {
                    results.append_range(nsdms_accessible_from<nsdm_t>());
                }
                else
                {
                    results.push_back(nsdm);
                }
            }

            return std::define_static_array(results);
        }
        
        /// Compute the blueprint of the provided constant value.
        template<class T>
        consteval Blueprint blueprint_of(T value);

        /// Compute the assignment of the provided constant value for the given subobject.
        template<std::meta::info Subobject, class T>
        consteval Blueprint::Assign assignment_of(T value)
        {
            constexpr auto SubobjectT = std::meta::type_of(Subobject);

            // If we can store the desired subobject state in a template parameter,
            // do so!
            if constexpr(std::meta::is_structural_type(SubobjectT) && !std::meta::is_base(Subobject))
            {
                return {
                    .field = Subobject,
                    .value = std::meta::reflect_constant(access_subobject_of<Subobject>(value))
                };
            }

            // Otherwise, create a nested blueprint to get ourselves to the desired state.
            else
            {
                // If we are modifying an anonymous union, we need to make sure that we
                // assign the active member before modifying its components. This requires
                // us to call the active member's default constructor, which limits the number
                // of types that our system can handle. To avoid the spread of this, we only
                // perform this default construction on fields that reside in a union.
                auto bp = blueprint_of(access_subobject_of<Subobject>(value));
                if (std::meta::is_union_type(std::meta::parent_of(Subobject)))
                {
                    bp.reconstruct_before_assignments = true;
                }

                return {
                    .field = Subobject,
                    .value = std::meta::reflect_constant(bp)
                };
            }
        };

        // Definition
        template<class T>
        consteval Blueprint blueprint_of(T value)
        {
            std::vector<Blueprint::Assign> assignments;
            
            template for(constexpr auto base : std::define_static_array(
                std::meta::bases_of(^^T, std::meta::access_context::unchecked())))
            {
                assignments.push_back(assignment_of<base>(value));
            }

            template for(constexpr auto nsdm : nsdms_accessible_from<^^T>())
            {
                // We need to avoid trying to save data in fields that are not
                // within their lifetime; i.e. non-active union members. In order
                // to do this, we must use std::is_within_lifetime, which requires
                // a pointer to the member. We cannot, however, get a pointer to a
                // bitfield, so we must special-case those here.
                auto within_lifetime = true;
                if constexpr(!std::meta::is_bit_field(nsdm))
                {
                    within_lifetime = is_within_lifetime(&value.[:nsdm:]);
                }

                if(within_lifetime)
                {
                    assignments.push_back(assignment_of<nsdm>(value));
                }
            }

            return {
                .assignments = std::define_static_array(assignments).data(),
                .count = assignments.size(),
                .reconstruct_before_assignments = false,
                .type = ^^T
            };
        }

        /// A wrapper around execute_blueprint which returns the object that the blueprint
        /// represents without any fussy API.
        template<Blueprint B>
        consteval auto reify_blueprint() -> [:B.type:]
        {
            typename [:B.type:] result;

            execute_blueprint<B>(result);
            return result;
        }

        /// The template that will generate static objects via blueprint.
        template<Blueprint BP>
        constexpr typename [:BP.type:] BlueprintedObject[1] = { reify_blueprint<BP>() };

        /// The template that will generate static arrays via blueprint.
        template<Blueprint ...BPs>
        constexpr typename [:BPs...[0].type:] BlueprintedArray[sizeof...(BPs)] = { reify_blueprint<BPs>()... };
    }
    
    /// Define a static object of (essentially) any constexpr-usable, constexpr-default-constructible 
    /// type. This object will be placed into static storage, and can be used for constant evaluation.
    template<class T>
    consteval auto define_any_static_object(const T &object) -> const T *
    {
        if constexpr(std::meta::is_structural_type(^^T))
        {
            return &std::define_static_array((std::vector<T>){object})[0];
        }
        else
        {
            auto bp = blueprint_of(object);
            auto obj = std::meta::substitute(^^detail::blueprints::BlueprintedObject, { std::meta::reflect_constant(bp) });
            return std::meta::extract<const T *>(obj);
        }
    }

    /// Define a static array of (essentially) any constexpr-usable, constexpr-default-constructible 
    /// type. This array will be placed into static storage, and can be used for constant evaluation.
    template<class R>
    consteval auto define_any_static_array(R &&range) -> std::span<const std::ranges::range_value_t<R>>
    {
        using T = std::ranges::range_value_t<R>;

        if constexpr(std::meta::is_structural_type(^^T))
        {
            return std::define_static_array(FORWARD(range));
        }
        else
        {
            std::vector<std::meta::info> values;

            for(auto value : range)
                values.push_back(std::meta::reflect_constant(dib::detail::blueprints::blueprint_of(value)));

            if(values.size() > 0)
            {
                auto arr = std::meta::substitute(^^dib::detail::blueprints::BlueprintedArray, values);
                auto arr_ptr = std::meta::extract<const T*>(arr); 

                return { arr_ptr, values.size() };
            }
            else return {};
        }
    }

    consteval bool has_annotation(std::meta::info refl, std::meta::info type, bool include_bases=true)
    {
        auto result = std::ranges::any_of(std::meta::annotations_of(refl), 
            [&](auto &&annotation) 
            { 
                return std::meta::is_assignable_type(type, std::meta::type_of(annotation)); 
            });

        if(include_bases && !result && std::meta::is_type(refl) && std::meta::is_class_type(refl))
        {
            result = std::ranges::any_of(std::meta::bases_of(refl, std::meta::access_context::unprivileged()),
                [&](auto &&base)
                {
                    return has_annotation(std::meta::type_of(base), type, include_bases);
                });
        }

        return result;
    }

    template<class T, class ...A>
    concept annotated_with = (has_annotation(^^T, ^^A, true) || ...);
    template<class T, class ...A>
    concept annotated_directly_with = (has_annotation(^^T, ^^A, false) || ...);

}