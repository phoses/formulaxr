"""Import the Fab "Toy Racing Car Low-poly PBR" and create the empty track map.

Run through Scripts/import_toy_car.ps1 (copies the source files, then runs this in the editor):
    UnrealEditor-Cmd.exe FormulaXR.uproject -ExecutePythonScript=Scripts/import_toy_car.py -unattended

Creates under /Game/Fab/ToyRacingCar:
    SM_ToyCar_Body, SM_ToyCar_Wheels          static meshes (authored in cm, 183 x 107 x 71)
    Textures/car_*                              PBR textures, capped at 2048
    M_ToyCar                                    PBR material + body recolour (docs/plan.md §6.1)
    MI_ToyCar_Red, MI_ToyCar_Blue              car 1 (original red) and car 2
and the empty map /Game/FormulaXR/Maps/L_Track (the game mode builds the scene at runtime).
Prints TOYCAR_IMPORT_OK on success.
"""
import os
import traceback

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SOURCE = os.path.join(ROOT, "SourceArt", "ToyRacingCar")
CAR_PATH = "/Game/Fab/ToyRacingCar"
TEX_PATH = CAR_PATH + "/Textures"
MAP_PATH = "/Game/FormulaXR/Maps/L_Track"

BLUE = (0.02, 0.18, 1.0)
# Red mask: R - max(G, B) above this counts as body paint.
RED_THRESHOLD = 0.25
RED_SHARPNESS = 4.0

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def log(message):
    unreal.log(message)


# ---------------------------------------------------------------- meshes

def import_meshes():
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    data = options.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", False)
    data.set_editor_property("generate_lightmap_u_vs", False)
    data.set_editor_property("auto_generate_collision", False)

    task = unreal.AssetImportTask()
    task.filename = os.path.join(SOURCE, "car.fbx")
    task.destination_path = CAR_PATH
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    asset_tools.import_asset_tasks([task])

    meshes = {}
    for path in task.get_editor_property("imported_object_paths"):
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.StaticMesh):
            target = "SM_ToyCar_Wheels" if "wheel" in asset.get_name().lower() else "SM_ToyCar_Body"
            new_path = f"{CAR_PATH}/{target}"
            if asset.get_path_name().split(".")[0] != new_path:
                eal.rename_asset(asset.get_path_name().split(".")[0], new_path)
            meshes[target] = unreal.load_asset(new_path)
    for name in ("SM_ToyCar_Body", "SM_ToyCar_Wheels"):
        if name not in meshes:
            raise RuntimeError(f"{name} not imported; got {list(meshes)}")
        bounds = meshes[name].get_bounds()
        log(f"TOYCAR {name} origin={bounds.origin} extent={bounds.box_extent}")
        # The game splits the wheels into front/rear axles at runtime, which reads the mesh on the CPU.
        meshes[name].set_editor_property("allow_cpu_access", True)
        eal.save_loaded_asset(meshes[name])
    return meshes


# ---------------------------------------------------------------- textures

def import_texture(file_name, srgb, compression):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SOURCE, "textures", file_name)
    task.destination_path = TEX_PATH
    task.automated = True
    task.replace_existing = True
    task.save = True
    asset_tools.import_asset_tasks([task])
    texture = unreal.load_asset(f"{TEX_PATH}/{os.path.splitext(file_name)[0]}")
    if not texture:
        raise RuntimeError(f"Texture import failed: {file_name}")
    texture.set_editor_property("srgb", srgb)
    texture.set_editor_property("compression_settings", compression)
    texture.set_editor_property("max_texture_size", 2048)
    eal.save_loaded_asset(texture)
    return texture


# ---------------------------------------------------------------- material

def node(material, cls, x, y):
    return mel.create_material_expression(material, cls, x, y)


def constant(material, value, x, y):
    result = node(material, unreal.MaterialExpressionConstant, x, y)
    result.set_editor_property("r", value)
    return result


def sample(material, texture, sampler, x, y):
    result = node(material, unreal.MaterialExpressionTextureSample, x, y)
    result.set_editor_property("texture", texture)
    result.set_editor_property("sampler_type", sampler)
    return result


def mask(material, source, channel, x, y):
    result = node(material, unreal.MaterialExpressionComponentMask, x, y)
    for c in "rgba":
        result.set_editor_property(c, c == channel)
    mel.connect_material_expressions(source, "RGB", result, "")
    return result


def binary(material, cls, a, a_out, b, b_out, x, y):
    result = node(material, cls, x, y)
    mel.connect_material_expressions(a, a_out, result, "A")
    mel.connect_material_expressions(b, b_out, result, "B")
    return result


def build_material(textures):
    path = f"{CAR_PATH}/M_ToyCar"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    material = asset_tools.create_asset("M_ToyCar", CAR_PATH, unreal.Material, unreal.MaterialFactoryNew())
    P = unreal.MaterialProperty
    S = unreal.MaterialSamplerType

    albedo = sample(material, textures["albedo"], S.SAMPLERTYPE_COLOR, -1400, -300)
    red = mask(material, albedo, "r", -1100, -500)
    green = mask(material, albedo, "g", -1100, -400)
    blue = mask(material, albedo, "b", -1100, -300)

    # Body paint mask: saturate((R - max(G, B) - threshold) * sharpness)
    green_blue = binary(material, unreal.MaterialExpressionMax, green, "", blue, "", -950, -350)
    redness = binary(material, unreal.MaterialExpressionSubtract, red, "", green_blue, "", -800, -400)
    shifted = binary(material, unreal.MaterialExpressionSubtract, redness, "", constant(material, RED_THRESHOLD, -950, -200), "", -650, -400)
    scaled = binary(material, unreal.MaterialExpressionMultiply, shifted, "", constant(material, RED_SHARPNESS, -800, -200), "", -500, -400)
    paint = node(material, unreal.MaterialExpressionSaturate, -350, -400)
    mel.connect_material_expressions(scaled, "", paint, "")

    amount = node(material, unreal.MaterialExpressionScalarParameter, -500, -250)
    amount.set_editor_property("parameter_name", "RecolorAmount")
    amount.set_editor_property("default_value", 0.0)
    weight = binary(material, unreal.MaterialExpressionMultiply, paint, "", amount, "", -200, -350)

    # Recoloured paint keeps the painted shading: BodyColor * R.
    body_colour = node(material, unreal.MaterialExpressionVectorParameter, -500, -600)
    body_colour.set_editor_property("parameter_name", "BodyColor")
    body_colour.set_editor_property("default_value", unreal.LinearColor(BLUE[0], BLUE[1], BLUE[2], 1.0))
    repainted = binary(material, unreal.MaterialExpressionMultiply, body_colour, "", red, "", -200, -550)

    base = node(material, unreal.MaterialExpressionLinearInterpolate, -50, -400)
    mel.connect_material_expressions(albedo, "RGB", base, "A")
    mel.connect_material_expressions(repainted, "", base, "B")
    mel.connect_material_expressions(weight, "", base, "Alpha")
    mel.connect_material_property(base, "", P.MP_BASE_COLOR)

    normal = sample(material, textures["normal"], S.SAMPLERTYPE_NORMAL, -400, 0)
    mel.connect_material_property(normal, "RGB", P.MP_NORMAL)
    for key, prop, y in (("roughness", P.MP_ROUGHNESS, 250), ("metallic", P.MP_METALLIC, 500), ("ao", P.MP_AMBIENT_OCCLUSION, 750)):
        tex = sample(material, textures[key], S.SAMPLERTYPE_MASKS, -400, y)
        mel.connect_material_property(tex, "R", prop)

    mel.recompile_material(material)
    eal.save_loaded_asset(material)
    return material


def make_instance(name, parent, recolor, colour):
    path = f"{CAR_PATH}/{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    instance = asset_tools.create_asset(name, CAR_PATH, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, parent)
    mel.set_material_instance_scalar_parameter_value(instance, "RecolorAmount", recolor)
    mel.set_material_instance_vector_parameter_value(instance, "BodyColor", unreal.LinearColor(colour[0], colour[1], colour[2], 1.0))
    mel.update_material_instance(instance)
    eal.save_loaded_asset(instance)


# ---------------------------------------------------------------- map

def ensure_map():
    if eal.does_asset_exist(MAP_PATH):
        log(f"TOYCAR map exists: {MAP_PATH}")
        return
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels.new_level(MAP_PATH):
        raise RuntimeError(f"Could not create {MAP_PATH}")
    levels.save_current_level()
    log(f"TOYCAR map created: {MAP_PATH}")


def main():
    if eal.does_directory_exist(CAR_PATH):
        eal.delete_directory(CAR_PATH)
    import_meshes()
    C = unreal.TextureCompressionSettings
    textures = {
        "albedo": import_texture("car_Albedo.png", True, C.TC_DEFAULT),
        "normal": import_texture("car_Normal.png", False, C.TC_NORMALMAP),
        "roughness": import_texture("car_Roughness.png", False, C.TC_MASKS),
        "metallic": import_texture("car_Metallic.png", False, C.TC_MASKS),
        "ao": import_texture("car_AO.png", False, C.TC_MASKS),
    }
    material = build_material(textures)
    make_instance("MI_ToyCar_Red", material, 0.0, BLUE)
    make_instance("MI_ToyCar_Blue", material, 1.0, BLUE)
    ensure_map()
    log("TOYCAR_IMPORT_OK")


try:
    main()
except Exception:
    unreal.log_error("TOYCAR_IMPORT_FAILED\n" + traceback.format_exc())
