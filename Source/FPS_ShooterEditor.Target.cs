using UnrealBuildTool;
using System.Collections.Generic;

public class FPS_ShooterEditorTarget : TargetRules
{
	public FPS_ShooterEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("FPS_Shooter");
	}
}
