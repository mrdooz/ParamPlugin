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

#define DESCRIPTION "\nParamPlugin - Magnus Osterlind, 2015/6"

#define NAME "ParamPlugin"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define BUG_VERSION 0
#define STAGE_VERSION PF_Stage_DEVELOP
#define BUILD_VERSION 1

enum class ControllerItems
{
  CONTROLLER_ZERO,
  CONTROLLER_ADD_1D,
  CONTROLLER_ADD_2D,
  CONTROLLER_ADD_3D,
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

template <typename T>
int CopyToBuffer(char* buf, const T& t)
{
  memcpy(buf, &t, sizeof(t));
  return sizeof(t);
}

struct Value
{
  Value() : x(0), y(0), z(0) {}
  Value(float x, float y = 0, float z = 0) : x(x), y(y), z(z) {}

  friend bool operator==(const Value& lhs, const Value& rhs)
  {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
  }

  float x, y, z;
};

struct Keyframes
{
  string name;
  int dims = 0;
  Value firstValue;
  Value lastValue;
  float firstTime = 0;
  float lastTime = 0;
  float sampleStep;
  vector<Value> values;

  friend bool operator==(const Keyframes& lhs, const Keyframes& rhs) { return lhs.values == rhs.values; }

  friend bool operator!=(const Keyframes& lhs, const Keyframes& rhs) { return !(lhs == rhs); }

  int ToBuffer(char* buf) const;
};

extern unordered_map<string, Keyframes> g_keyframes;
extern unordered_set<string> g_newKeyframes;
