using System.IO;
using UnrealBuildTool;

public class FormulaXR : ModuleRules
{
	public FormulaXR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "HeadMountedDisplay", "XRBase", "ProceduralMeshComponent", "UMG", "Slate", "SlateCore", "RHI", "RenderCore"
		});

		// Meta XR (Plugins/MetaXR, see docs/plan.md §7) supplies the Quest passthrough layer.
		// Without it, -vr still starts the headset, but in full VR instead of mixed reality.
		bool bMetaXR = File.Exists(Path.Combine(ModuleDirectory, "..", "..", "Plugins", "MetaXR", "OculusXR.uplugin"));
		if (bMetaXR)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "OculusXRHMD", "OculusXRPassthrough" });
		}
		PublicDefinitions.Add("WITH_META_XR=" + (bMetaXR ? "1" : "0"));
	}
}
