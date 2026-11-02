#include <string_view>

#include <vtablespp/vtablespp.hpp>

constexpr std::size_t inspected_slot_count = 1;

template <class T>
void print_table(std::string_view label, const T& object) {
    const auto view = vtablespp::vtable_view::from_object(
        object,
        inspected_slot_count
    );

    std::cout << "\n[" << label << "]\n";

    std::cout << "  Object/subobject address: "
              << static_cast<const void*>(std::addressof(object))
              << '\n';

    std::cout << "  Vftable address:          "
              << static_cast<const void*>(view.entries().data())
              << '\n';

    std::cout << "  Inspected slots:          "
              << view.size()
              << '\n';

    for (std::size_t index = 0; index < view.size(); ++index) {
        std::cout << "  slot[" << index << "]:                  "
                  << view.at(index)
                  << '\n';
    }
}