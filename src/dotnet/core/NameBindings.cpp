#include <system/Debug.hpp>

#include <Engine.hpp>
#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
static_assert(sizeof(Name) == 8, "Name size mismatch, ensure C# implementation matches C++");
static_assert(std::is_standard_layout_v<Name>, "Name is not standard layout");

HYP_EXPORT uint64 Name_FromString(const char *str)
{
    if (str == nullptr) {
        return 0;
    }

    return CreateNameFromDynamicString(str).hash_code;
}
} // extern "C"