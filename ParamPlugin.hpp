#pragma once

#include "AEConfig.h"
#include "entry.h"
#include "AEFX_SuiteHelper.h"
#include "PrSDKAESupport.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_Macros.h"
#include "AEGP_SuiteHandler.h"
#include "String_Utils.h"
#include "Param_Utils.h"
#include "Smart_Utils.h"

#ifdef AE_OS_WIN
#include <Windows.h>
#endif

#define DESCRIPTION "\nParamPlugin - Magnus Osterlind, 2015."

#define NAME "ParamPlugin"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define BUG_VERSION 0
#define STAGE_VERSION PF_Stage_DEVELOP
#define BUILD_VERSION 1

enum class ControllerItems
{
  CONTROLLER_ZERO,
  CONTROLLER_FLAVOR,
  CONTROLLER_NUM_ITEMS,
};

enum class InstanceItems
{
  INSTANCE_ZERO,
  INSTANCE_SLIDER_1,
  INSTANCE_NUM_ITEMS,
};

enum class InstanceType
{
  INSTANCE_ZERO,
  INSTANCE_FLOAT,
  INSTANCE_FLOAT_2D,
  INSTANCE_FLOAT_3D,
  INSTANCE_SIZE = INSTANCE_FLOAT_3D
};

#define SLIDER_MIN 0
#define SLIDER_MAX 100

#define RESTRICT_BOUNDS 0
#define SLIDER_PRECISION 1

extern "C" {

DllExport PF_Err EntryPointFuncController(PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data,
    PF_ParamDef* params[], PF_LayerDef* output, void* extra);

DllExport PF_Err EntryPointFuncInstance1d(PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data,
    PF_ParamDef* params[], PF_LayerDef* output, void* extra);

DllExport PF_Err EntryPointFuncInstance2d(PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data,
    PF_ParamDef* params[], PF_LayerDef* output, void* extra);

DllExport PF_Err EntryPointFuncInstance3d(PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data,
    PF_ParamDef* params[], PF_LayerDef* output, void* extra);
}
