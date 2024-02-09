#include <system/Debug.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
    uint32 TypeID_ForDynamicType(const char *type_name)
    {
        const TypeID type_id = TypeID::ForName(CreateNameFromDynamicString(type_name));

        return type_id.Value();
    }
}