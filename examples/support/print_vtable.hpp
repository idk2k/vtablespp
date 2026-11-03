#pragma once

#include <cstddef>
#include <iostream>
#include <string_view>

#include <vtablespp/vtablespp.hpp>

namespace vtablespp_example {

template <class T>
void print_object_vtable(
    std::string_view label,
    const T& object,
    std::size_t slot_count
) {
    const auto view = vtablespp::vtable_view::from_object(object, slot_count);

    std::cout << "\n[" << label << "]\n";
    
    std::cout << "  Object/subobject address: ";
    std::cout << static_cast<const void*>(std::addressof(object)) << '\n';
    
    std::cout << "  Vftable address: ";
    std::cout << static_cast<const void*>(view.entries().data()) << '\n';

    std::cout << "  Inspected slots: " << view.size() << '\n';

    for (std::size_t index = 0; index < view.size(); ++index) {
        std::cout << "  slot[" << index << "]: ";
        std::cout << view.at(index) << '\n';
    }
}

} // namespace vtablespp_example
