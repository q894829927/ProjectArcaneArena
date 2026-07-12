#pragma once

#include "CoreMinimal.h"

enum class EArenaNetworkAbilityId : uint8
{
	None = 0,
	BasicAttack = 1,
	Fireball = 2,
	Dash = 3,
	Shield = 4,
	LightningStorm = 5
};

DECLARE_LOG_CATEGORY_EXTERN(LogArenaAbilityNet, Log, All);

namespace ArenaAbilityNetworkDebug
{
	// 仅在开发构建的服务器上消费一次对应技能拒绝请求，用于验证 GAS 预测回滚。
	PROJECTARCANEARENA_API bool ConsumeServerRejection(EArenaNetworkAbilityId AbilityId);

	// 查询网络审计开关，避免正常运行时产生高频技能日志。
	PROJECTARCANEARENA_API bool IsAuditEnabled();

	// 为服务器权威执行生成单调序号，便于核对一次激活只产生一次结果。
	PROJECTARCANEARENA_API uint64 NextServerExecutionSequence();
}
