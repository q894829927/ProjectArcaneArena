// Copyright 2022 (c) Microsoft. All rights reserved.
// Licensed under the MIT License.

#include "VisualStudioTools.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogVisualStudioTools);

class FVisualStudioToolsModule : public IModuleInterface
{
public:
	// 模块启动入口，当前插件不需要额外初始化。
	virtual void StartupModule() override {}
	// 模块关闭入口，当前插件不需要额外清理。
	virtual void ShutdownModule() override {}
};

// 注册 VisualStudioTools 编辑器插件模块。
IMPLEMENT_MODULE(FVisualStudioToolsModule, VisualStudioTools)
