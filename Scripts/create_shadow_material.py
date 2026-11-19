"""Create /Game/FormulaXR/Materials/M_SoftShadow for mixed reality grounding (docs/plan.md §4.6).

Translucent, unlit black whose opacity is the vertex alpha times a Strength parameter, so soft
shadow shapes are drawn as procedural meshes with alpha fading to 0 at their edges. Over
passthrough (r.PostProcessing.PropagateAlpha) it darkens the real table.

Run through Scripts/run_editor_script.ps1 create_shadow_material SHADOWMAT_OK.
"""
import traceback

import unreal

PATH = "/Game/FormulaXR/Materials"
NAME = "M_SoftShadow"

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def main():
    full = f"{PATH}/{NAME}"
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    P = unreal.MaterialProperty

    black = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, 0)
    black.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
    mel.connect_material_property(black, "", P.MP_EMISSIVE_COLOR)

    vertex = mel.create_material_expression(material, unreal.MaterialExpressionVertexColor, -600, 200)
    strength = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -600, 350)
    strength.set_editor_property("parameter_name", "Strength")
    strength.set_editor_property("default_value", 0.6)
    multiply = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -350, 250)
    mel.connect_material_expressions(vertex, "A", multiply, "A")
    mel.connect_material_expressions(strength, "", multiply, "B")
    mel.connect_material_property(multiply, "", P.MP_OPACITY)

    mel.recompile_material(material)
    eal.save_loaded_asset(material)
    unreal.log("SHADOWMAT_OK")


try:
    main()
except Exception:
    unreal.log_error("SHADOWMAT_FAILED\n" + traceback.format_exc())
