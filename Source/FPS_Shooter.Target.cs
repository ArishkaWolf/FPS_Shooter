using UnrealBuildTool;
using System.Collections.Generic;

public class FPS_ShooterTarget : TargetRules
{
	public FPS_ShooterTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("FPS_Shooter");
	}
}
