/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/Assets.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT const AssetLoaderDefinition *AssetManager_GetLoaderDefinition(AssetManager *asset_manager, const char *path, TypeID desired_type_id)
{
    AssertThrow(asset_manager != nullptr);

    return asset_manager->GetLoaderDefinition(path, desired_type_id);
}

HYP_EXPORT LoadedAsset *AssetManager_Load(AssetManager *asset_manager, AssetLoaderDefinition *loader_definition, const char *path)
{
    AssertThrow(asset_manager != nullptr);
    AssertThrow(loader_definition != nullptr);

    AssetLoaderBase *loader = loader_definition->loader.Get();

    if (!loader) {
        return nullptr;
    }

    return new LoadedAsset(loader->Load(*asset_manager, path));
}

} // extern "C"