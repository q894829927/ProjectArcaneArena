#include "Core/ArenaWaveDataAsset.h"

#include "GAS/ArenaGameplayEffect_EliteBaseline.h"

// 为旧 WaveData 资产提供原生精英基础 GE 后备，资产脚本仍会显式保存该引用。
UArenaWaveDataAsset::UArenaWaveDataAsset()
{
	EliteBaselineEffectClass = UArenaGameplayEffect_EliteBaseline::StaticClass();
}
