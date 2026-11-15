using UnrealBuildTool;
using System.Collections.Generic;

public class SoundSystemEditorTarget : TargetRules
{
	public SoundSystemEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("SoundSystem");
	}
}
