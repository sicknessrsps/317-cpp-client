#include "SceneLoc.h"
#include "Scene.h"
#include "Entity.h"

namespace SDL_Client
{
    bool SceneLoc::Drawn() const
    {
        return cycle == Scene::cycle;
    }

}
