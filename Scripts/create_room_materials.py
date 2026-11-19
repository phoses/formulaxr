"""Create the PC setting's materials (floor and table): one solid-colour master plus an instance per surface.

Run through Scripts/run_editor_script.ps1 create_room_materials ROOMMAT_OK.
"""
import traceback

import unreal

PATH = "/Game/FormulaXR/Materials"
# name: (linear colour, roughness)
SURFACES = {
    "MI_RoomFloor": ((0.28, 0.16, 0.08), 0.6),     # oak floor
    "MI_TableTop": ((0.12, 0.06, 0.03), 0.45),     # dark walnut
    "MI_TableLeg": ((0.03, 0.03, 0.03), 0.4),      # black metal
}

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def fresh(name, cls, factory):
    path = f"{PATH}/{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return tools.create_asset(name, PATH, cls, factory)


def main():
    master = fresh("M_Solid", unreal.Material, unreal.MaterialFactoryNew())
    P = unreal.MaterialProperty
    colour = mel.create_material_expression(master, unreal.MaterialExpressionVectorParameter, -400, 0)
    colour.set_editor_property("parameter_name", "Color")
    colour.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    mel.connect_material_property(colour, "", P.MP_BASE_COLOR)
    roughness = mel.create_material_expression(master, unreal.MaterialExpressionScalarParameter, -400, 200)
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.6)
    mel.connect_material_property(roughness, "", P.MP_ROUGHNESS)
    mel.recompile_material(master)
    eal.save_loaded_asset(master)

    for name, (rgb, rough) in SURFACES.items():
        instance = fresh(name, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(instance, master)
        mel.set_material_instance_vector_parameter_value(instance, "Color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        mel.set_material_instance_scalar_parameter_value(instance, "Roughness", rough)
        mel.update_material_instance(instance)
        eal.save_loaded_asset(instance)
    unreal.log("ROOMMAT_OK")


try:
    main()
except Exception:
    unreal.log_error("ROOMMAT_FAILED\n" + traceback.format_exc())
