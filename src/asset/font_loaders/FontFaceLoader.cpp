/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/font_loaders/FontFaceLoader.hpp>

namespace hyperion {

LoadedAsset FontFaceLoader::LoadAsset(LoaderState &state) const
{
    FontEngine &font_engine = FontEngine::GetInstance();

    RC<FontFace> font_face = MakeRefCountedPtr<FontFace>(
        font_engine.GetFontBackend(),
        state.filepath
    );

    return { { LoaderResult::Status::OK }, font_face };
}

} // namespace hyperion
