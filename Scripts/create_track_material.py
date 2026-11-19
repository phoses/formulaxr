"""Create /Game/FormulaXR/Materials/M_TrackVertexColor: the track mesh's colours come from its vertices.

Run through Scripts/run_editor_script.ps1 create_track_material TRACKMAT_OK.
"""
import traceback

import unreal

PATH = "/Game/FormulaXR/Materials"
NAME = "M_TrackVertexColor"

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def main():
    full = f"{PATH}/{NAME}"
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("two_sided", True)
    P = unreal.MaterialProperty

    colour = mel.create_material_expression(material, unreal.MaterialExpressionVertexColor, -400, 0)
    mel.connect_material_property(colour, "", P.MP_BASE_COLOR)
    roughness = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 200)
    roughness.set_editor_property("r", 0.55)
    mel.connect_material_property(roughness, "", P.MP_ROUGHNESS)
    specular = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 300)
    specular.set_editor_property("r", 0.35)
    mel.connect_material_property(specular, "", P.MP_SPECULAR)

    mel.recompile_material(material)
    eal.save_loaded_asset(material)
    unreal.log("TRACKMAT_OK")


try:
    main()
except Exception:
    unreal.log_error("TRACKMAT_FAILED\n" + traceback.format_exc())
