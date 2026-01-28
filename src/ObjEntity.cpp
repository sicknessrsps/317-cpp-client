#include "ObjEntity.h"

#include "Model.h"
#include "ObjType.h"

namespace SDL_Client
{
    std::shared_ptr<Model> ObjEntity::GetModel()
    {
        auto type = ObjType::Get(id);
        return type->GetModel(count);
    }
}
