"""Import the generated sound effects (Build/audio, from generate_sounds.py) to /Game/FormulaXR/Audio.

Run through Scripts/import_sounds.ps1 (generates, then imports).
"""
import os
import traceback

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SOURCE = os.path.join(ROOT, "Build", "audio")
DEST = "/Game/FormulaXR/Audio"
SOUNDS = {
    # wav name: (asset name, looping)
    "motor_loop": ("SW_MotorLoop", True),
    "deslot": ("SW_Deslot", False),
    "light_beep": ("SW_LightBeep", False),
    "go_beep": ("SW_GoBeep", False),
    "finish": ("SW_Finish", False),
}

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def main():
    for wav, (name, looping) in SOUNDS.items():
        path = f"{DEST}/{name}"
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
        task = unreal.AssetImportTask()
        task.filename = os.path.join(SOURCE, f"{wav}.wav")
        task.destination_path = DEST
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = True
        tools.import_asset_tasks([task])
        sound = unreal.load_asset(path)
        if not sound:
            raise RuntimeError(f"Import failed: {wav}.wav")
        sound.set_editor_property("looping", looping)
        eal.save_loaded_asset(sound)
        unreal.log(f"SOUND {path} looping={looping} duration={sound.get_editor_property('duration'):.2f}s")
    unreal.log("SOUNDS_IMPORT_OK")


try:
    main()
except Exception:
    unreal.log_error("SOUNDS_IMPORT_FAILED\n" + traceback.format_exc())
