#pragma once

#include "CoreMinimal.h"

// Boss 技能、区域 Actor 和战斗生命周期共用的项目日志分类，便于独立筛选与遥测。
DECLARE_LOG_CATEGORY_EXTERN(LogArenaBoss, Log, All);

// 阶段六 B 服务器平衡统计与 CSV 写入共用分类，便于和玩法日志独立筛选。
DECLARE_LOG_CATEGORY_EXTERN(LogArenaBalance, Log, All);
