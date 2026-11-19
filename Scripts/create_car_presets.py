"""Create the two car presets (docs/plan.md §6.3): the Fab toy car in red (car 1) and blue (car 2).

Run through Scripts/run_editor_script.ps1 create_car_presets CARPRESETS_OK, after import_toy_car.ps1.
"""
import traceback

import unreal

FOLDER = "/Game/FormulaXR/Cars"
CAR = "/Game/Fab/ToyRacingCar"
PRESETS = {
    "DA_Car1_Red": f"{CAR}/MI_ToyCar_Red",
    "DA_Car2_Blue": f"{CAR}/MI_ToyCar_Blue",
}

eal = unreal.EditorAssetLibrary


def main():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    body = unreal.load_asset(f"{CAR}/SM_ToyCar_Body")
    wheels = unreal.load_asset(f"{CAR}/SM_ToyCar_Wheels")
    if not body or not wheels:
        raise RuntimeError("Toy car meshes missing; run Scripts/import_toy_car.ps1 first")
    for name, paint_path in PRESETS.items():
        path = f"{FOLDER}/{name}"
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.FXRCarPreset)
        preset = tools.create_asset(name, FOLDER, unreal.FXRCarPreset, factory)
        preset.set_editor_property("body_mesh", body)
        preset.set_editor_property("wheels_mesh", wheels)
        preset.set_editor_property("paint", unreal.load_asset(paint_path))
        eal.save_loaded_asset(preset)
        unreal.log(f"CARPRESET {path}")
    unreal.log("CARPRESETS_OK")


try:
    main()
except Exception:
    unreal.log_error("CARPRESETS_FAILED\n" + traceback.format_exc())
