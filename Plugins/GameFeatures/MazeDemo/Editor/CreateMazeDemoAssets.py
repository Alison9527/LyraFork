"""
Create the required Content assets for the MazeDemo GameFeature plugin.

WHAT THIS CREATES
    1. /MazeDemo/GameFeatureData
         Engine prefers /<Plugin>/GameFeatureData.GameFeatureData, and falls back to
         /<Plugin>/<Plugin>.<Plugin>. UGameFeaturePolicy::ShouldAllowMissingGameFeatureData()
         returns false by default, so this asset is MANDATORY - the plugin will not
         register without it.
    2. /MazeDemo/Experiences/B_MazeDemo_Experience  (ULyraExperienceDefinition)
         Points at the ShooterCore hero data and enables the "MazeDemo" feature.

HOW TO RUN
    In the editor:  Tools > Execute Python Script > pick this file
    Headless:       UnrealEditor-Cmd.exe <path>/LyraFork.uproject -ExecutePythonScript="<abs path to this file>"

PREREQUISITE
    The plugin's C++ module must compile first. Build once before running this,
    otherwise the editor refuses to mount the plugin and these assets would have
    nothing to bind to.

Note: the level itself is NOT created here - making a good level needs a floor,
lights and a LyraPlayerStart, which is much more reliable to do by hand. See steps
in the companion chat message, or File > New Level > Basic, then Save As
/MazeDemo/Content/Maps/L_MazeBasic.
"""

import unreal

PLUGIN_NAME = "MazeDemo"
PLUGIN_CONTENT_ROOT = "/MazeDemo"

GAME_FEATURE_DATA_NAME = "GameFeatureData"
EXPERIENCE_PACKAGE = "/MazeDemo/Experiences"
EXPERIENCE_NAME = "B_MazeDemo_Experience"

# Reuse ShooterCore's hero for now. Replace with a MazeDemo-owned
# HeroData_MazeDemo once the mode gets its own abilities/equipment.
FALLBACK_HERO_DATA_PATHS = [
    "/ShooterCore/Game/HeroData_ShooterGame.HeroData_ShooterGame",
    "/ShooterTests/Game/HeroData_ShooterTests.HeroData_ShooterTests",
]


def ensure_data_asset(asset_name, package_path, asset_class):
    """Create a UDataAsset-derived asset, or return the existing one."""
    full_path = "{}/{}".format(package_path, asset_name)

    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        unreal.log("MazeDemo: {} already exists, reusing it".format(full_path))
        return unreal.EditorAssetLibrary.load_asset(full_path)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    try:
        factory.set_editor_property("data_asset_class", asset_class)
    except Exception as exc:  # noqa: BLE001 - surface and keep going with default factory behaviour
        unreal.log_warning("MazeDemo: could not set factory class ({}), trying anyway".format(exc))

    asset = asset_tools.create_asset(asset_name, package_path, asset_class, factory)
    if asset is None:
        unreal.log_error("MazeDemo: FAILED to create {}".format(full_path))
        return None

    unreal.log("MazeDemo: created {}".format(full_path))
    return asset


def resolve_pawn_data():
    for candidate in FALLBACK_HERO_DATA_PATHS:
        if unreal.EditorAssetLibrary.does_asset_exist(candidate):
            asset = unreal.EditorAssetLibrary.load_asset(candidate)
            unreal.log("MazeDemo: using default pawn data {}".format(candidate))
            return asset

    unreal.log_warning(
        "MazeDemo: no ULyraPawnData found in the project. "
        "Set DefaultPawnData on {} by hand before playing.".format(EXPERIENCE_NAME)
    )
    return None


def main():
    game_feature_data = ensure_data_asset(GAME_FEATURE_DATA_NAME, PLUGIN_CONTENT_ROOT, unreal.GameFeatureData)
    experience = ensure_data_asset(EXPERIENCE_NAME, EXPERIENCE_PACKAGE, unreal.LyraExperienceDefinition)

    if experience is not None:
        experience.set_editor_property("game_features_to_enable", [PLUGIN_NAME])

        pawn_data = resolve_pawn_data()
        if pawn_data is not None:
            experience.set_editor_property("default_pawn_data", pawn_data)

        unreal.EditorAssetLibrary.save_asset(
            "{}/{}".format(EXPERIENCE_PACKAGE, EXPERIENCE_NAME), only_if_is_dirty=False
        )

    if game_feature_data is not None:
        unreal.EditorAssetLibrary.save_asset(
            "{}/{}".format(PLUGIN_CONTENT_ROOT, GAME_FEATURE_DATA_NAME), only_if_is_dirty=False
        )

    unreal.log("MazeDemo: asset setup finished")


if __name__ == "__main__":
    main()
