//*****************************************************************************
// Copyright 2017-2019 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//*****************************************************************************

#pragma once

#include <memory>
#include <string>
#include <utility>

#include "ngraph/check.hpp"

namespace ngraph
{
    /// Supports three functions, is_type<Type>, as_type<Type>, and as_type_ptr<Type> for type-safe
    /// dynamic conversions via static_cast/static_ptr_cast without using C++ RTTI.
    /// Type must have a static constexpr type_info member and a virtual get_type_info() member that
    /// returns a reference to its type_info member.

    /// Type information for a type system without inheritance; instances have exactly one type not
    /// related to any other type.
    struct DiscreteTypeInfo
    {
        const char* name;
        uint64_t version;

        bool is_castable(const DiscreteTypeInfo& target_type) const { return this == &target_type; }
        bool operator<(const DiscreteTypeInfo& b) const
        {
            std::string sa(name);
            std::string sb(b.name);
            return sa < sb || (sa == sb && version < b.version);
        }
    };

    /// \brief Tests if value is a pointer/shared_ptr that can be statically cast to a
    /// Type*/shared_ptr<Type>
    template <typename Type, typename Value>
    typename std::enable_if<
        std::is_convertible<
            decltype(std::declval<Value>()->get_type_info().is_castable(Type::type_info)),
            bool>::value,
        bool>::type
        is_type(Value value)
    {
        return value->get_type_info().is_castable(Type::type_info);
    }

    /// Casts a Value* to a Type* if it is of type Type, nullptr otherwise
    template <typename Type, typename Value>
    typename std::enable_if<
        std::is_convertible<decltype(static_cast<Type*>(std::declval<Value>())), Type*>::value,
        Type*>::type
        as_type(Value value)
    {
        return is_type<Type>(value) ? static_cast<Type*>(value) : nullptr;
    }

    /// Casts a std::shared_ptr<Value> to a std::shared_ptr<Type> if it is of type
    /// Type, nullptr otherwise
    template <typename Type, typename Value>
    typename std::enable_if<
        std::is_convertible<decltype(std::static_pointer_cast<Type>(std::declval<Value>())),
                            std::shared_ptr<Type>>::value,
        std::shared_ptr<Type>>::type
        as_type_ptr(Value value)
    {
        return is_type<Type>(value) ? std::static_pointer_cast<Type>(value)
                                    : std::shared_ptr<Type>();
    }

    template <typename EnumType>
    class EnumNames
    {
    public:
        static EnumType as_type(const std::string& name)
        {
            for (auto p : get().m_string_enums)
            {
                if (p.first == name)
                {
                    return p.second;
                }
            }
            NGRAPH_CHECK(false, "\"", name, "\"", " is not a member of enum ", get().m_enum_name);
        }

        static std::string as_type(EnumType e)
        {
            for (auto p : get().m_string_enums)
            {
                if (p.second == e)
                {
                    return p.first;
                }
            }
            NGRAPH_CHECK(false, " invalid member of enum ", get().m_enum_name);
        }

    private:
        EnumNames(const std::string& enum_name,
                  const std::vector<std::pair<std::string, EnumType>> string_enums)
            : m_enum_name(enum_name)
            , m_string_enums(string_enums)
        {
        }
        static EnumNames<EnumType>& get();

        const std::string m_enum_name;
        std::vector<std::pair<std::string, EnumType>> m_string_enums;
    };

    template <typename Type, typename Value>
    typename std::enable_if<std::is_convertible<Value, std::string>::value, Type>::type
        as_type(const Value& value)
    {
        return EnumNames<Type>::as_type(value);
    }

    template <typename Type, typename Value>
    typename std::enable_if<std::is_convertible<Type, std::string>::value, Type>::type
        as_type(Value value)
    {
        return EnumNames<Value>::as_type(value);
    }

    class VisitorAdapter
    {
    public:
        static constexpr DiscreteTypeInfo type_info{"VisitorAdapter", 0};
        virtual ~VisitorAdapter() {}
        virtual const DiscreteTypeInfo& get_type_info() const { return type_info; }
        virtual std::string get_string() const = 0;
        virtual void set_string(const std::string& value) const = 0;
    };

    template <typename Type>
    class TypeAdapter : public VisitorAdapter
    {
    public:
        operator Type&() const { return m_value; }
    protected:
        TypeAdapter(Type& value)
            : m_value(value)
        {
        }
        Type& m_value;
    };

    template <typename Type>
    class EnumAdapter : public TypeAdapter<Type>
    {
    public:
        EnumAdapter(Type& value)
            : TypeAdapter<Type>(value)
        {
        }
        static const DiscreteTypeInfo type_info;
        const DiscreteTypeInfo& get_type_info() const override { return type_info; }
        std::string get_string() const override
        {
            return as_type<std::string>(TypeAdapter<Type>::m_value);
        }
        void set_string(const std::string& value) const override
        {
            TypeAdapter<Type>::m_value = as_type<Type>(value);
        }
    };

    template <typename Type>
    class ObjectAdapter : public TypeAdapter<Type>
    {
    public:
        ObjectAdapter(Type& value)
            : TypeAdapter<Type>(value)
        {
        }
        static const DiscreteTypeInfo type_info;
        const DiscreteTypeInfo& get_type_info() const override { return type_info; }
        std::string get_string() const override { return "TODO"; }
        void set_string(const std::string& value) const override {}
    };

    /// Adapts references to a value to a reference to a string
    template <typename Type>
    class StringAdapter
    {
    public:
        StringAdapter(Type& value)
            : m_string(as_type<std::string>(value))
            , m_value(value)
        {
        }
        ~StringAdapter() { m_value = as_type<Type>(m_string); }
        operator std::string&() { return m_string; }
    private:
        std::string m_string;
        Type& m_value;
    };
}
