#include "AEConfig.h"
#include "AE_EffectVers.h"

resource 'PiPL' (16000) {
	{	/* array properties: 12 elements */
		/* [1] */
		Kind {
			AEEffect
		},
		/* [2] */
		Name {
			"ParamController"
		},
		/* [3] */
		Category {
			"dooz"
		},
	#ifdef AE_PROC_INTELx64
		CodeWin64X86 {"EntryPointFuncController"},
	#else
		CodeWin32X86 {"EntryPointFuncController"},
	#endif	
		/* [6] */
		AE_PiPL_Version {
			2,
			0
		},
		/* [7] */
		AE_Effect_Spec_Version {
			PF_PLUG_IN_VERSION,
			PF_PLUG_IN_SUBVERS
		},
		/* [8] */
		AE_Effect_Version {	
			524289	/* 1.0 */
		},
		/* [9] */
		AE_Effect_Info_Flags {
			0
		},
		/* [10] */
		AE_Effect_Global_OutFlags {
			0x2000404
		},
		AE_Effect_Global_OutFlags_2 {
			0
		},
		/* [11] */
		AE_Effect_Match_Name {
			"ADBE ParamController"
		},
		/* [12] */
		AE_Reserved_Info {
			0
		}
	}
};

resource 'PiPL' (16001) {
	{	/* array properties: 12 elements */
		/* [1] */
		Kind {
			AEEffect
		},
		/* [2] */
		Name {
			"ParamInstance1d"
		},
		/* [3] */
		Category {
			"dooz"
		},
	#ifdef AE_PROC_INTELx64
		CodeWin64X86 {"EntryPointFuncInstance1d"},
	#else
		CodeWin32X86 {"EntryPointFuncInstance1d"},
	#endif	
		/* [6] */
		AE_PiPL_Version {
			2,
			0
		},
		/* [7] */
		AE_Effect_Spec_Version {
			PF_PLUG_IN_VERSION,
			PF_PLUG_IN_SUBVERS
		},
		/* [8] */
		AE_Effect_Version {	
			524289	/* 1.0 */
		},
		/* [9] */
		AE_Effect_Info_Flags {
			0
		},
		/* [10] */
		AE_Effect_Global_OutFlags {
			0x2200404
		},
		AE_Effect_Global_OutFlags_2 {
			0
		},
		/* [11] */
		AE_Effect_Match_Name {
			"ADBE ParamInstance1d"
		},
		/* [12] */
		AE_Reserved_Info {
			0
		}
	}
};

resource 'PiPL' (16002) {
	{	/* array properties: 12 elements */
		/* [1] */
		Kind {
			AEEffect
		},
		/* [2] */
		Name {
			"ParamInstance2d"
		},
		/* [3] */
		Category {
			"dooz"
		},
	#ifdef AE_PROC_INTELx64
		CodeWin64X86 {"EntryPointFuncInstance2d"},
	#else
		CodeWin32X86 {"EntryPointFuncInstance2d"},
	#endif	
		/* [6] */
		AE_PiPL_Version {
			2,
			0
		},
		/* [7] */
		AE_Effect_Spec_Version {
			PF_PLUG_IN_VERSION,
			PF_PLUG_IN_SUBVERS
		},
		/* [8] */
		AE_Effect_Version {	
			524289	/* 1.0 */
		},
		/* [9] */
		AE_Effect_Info_Flags {
			0
		},
		/* [10] */
		AE_Effect_Global_OutFlags {
			0x2200404
		},
		AE_Effect_Global_OutFlags_2 {
			0
		},
		/* [11] */
		AE_Effect_Match_Name {
			"ADBE ParamInstance2d"
		},
		/* [12] */
		AE_Reserved_Info {
			0
		}
	}
};

resource 'PiPL' (16003) {
	{	/* array properties: 12 elements */
		/* [1] */
		Kind {
			AEEffect
		},
		/* [2] */
		Name {
			"ParamInstance3d"
		},
		/* [3] */
		Category {
			"dooz"
		},
	#ifdef AE_PROC_INTELx64
		CodeWin64X86 {"EntryPointFuncInstance3d"},
	#else
		CodeWin32X86 {"EntryPointFuncInstance3d"},
	#endif	
		/* [6] */
		AE_PiPL_Version {
			2,
			0
		},
		/* [7] */
		AE_Effect_Spec_Version {
			PF_PLUG_IN_VERSION,
			PF_PLUG_IN_SUBVERS
		},
		/* [8] */
		AE_Effect_Version {	
			524289	/* 1.0 */
		},
		/* [9] */
		AE_Effect_Info_Flags {
			0
		},
		/* [10] */
		AE_Effect_Global_OutFlags {
			0x2200404
		},
		AE_Effect_Global_OutFlags_2 {
			0
		},
		/* [11] */
		AE_Effect_Match_Name {
			"ADBE ParamInstance3d"
		},
		/* [12] */
		AE_Reserved_Info {
			0
		}
	}
};

