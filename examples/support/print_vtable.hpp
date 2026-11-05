#pragma once

#include <iomanip>
#include <ostream>
#include <string_view>

#include "vtablespp/core/slot_classifier.hpp"
#include "vtablespp/vtablespp.hpp"

namespace vtablespp_example {

inline void print_slot(
    std::ostream& os,
    std::size_t index,
    const vtablespp::slot_info& info
) {
    os << "  [" << index << "] entry="
       << info.entry
       << ", kind=";

    switch (info.kind) {
    case vtablespp::slot_kind::direct:
        os << "direct";
        break;
    case vtablespp::slot_kind::this_adjustor:
        os << "this_adjustor";
        break;
    case vtablespp::slot_kind::return_adjustor:
        os << "return_adjustor";
        break;
    case vtablespp::slot_kind::pure_call:
        os << "pure_call";
        break;
    case vtablespp::slot_kind::unknown:
        os << "unknown";
        break;
    }

    if (info.kind == vtablespp::slot_kind::this_adjustor) {
        os << ", this_adjustment=" << info.this_adjustment;
    }

    if (info.final_target.has_value()) {
        os << ", final_target=" << *info.final_target;
    } else {
        os << ", final_target=<none>";
    }

    os << '\n';
}

template <class T>
void print_object_vtable(
    std::string_view name,
    const T& object,
    std::size_t size
) {
    std::ostream& os = std::cout;
    const vtablespp::vtable_view view =
        vtablespp::vtable_view::from_object(object, size);

    const vtablespp::slot_classifier classifier{view};

    os << name << " vtable:\n";

    for (std::size_t i = 0; i < view.size(); ++i) {
        print_slot(os, i, classifier.inspect(i));
    }
}

} // namespace vtablespp_example