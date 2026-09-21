# MCP 对话导出

导出时间：2026-07-21

说明：本文根据当前会话中可见的上下文整理，按时间顺序记录用户问题、助手回答要点、执行过的仓库检查和产生的文件。它是工程交接用的 Markdown 对话导出，不包含平台内部元数据或不可见系统消息。

## 1. 仓库约束与背景

用户提供了 `E:\code\GWCPEngine` 的仓库工作约束：

- C++ 标准为 C++17。
- 当前 editor 是 ImGui-based `./editor` 树，忽略 Qt editor 路径。
- 当前 editor 构建入口以 `scripts\Editor.bat` 或 `./editor` 为准，不信任根 `CMakeLists.txt` 做 editor 工作。
- Windows editor CI 使用 `scripts\SetupCmakeEnv.bat` 和 `scripts\Editor.bat`。
- 当前 editor runtime 代码在 `editor/`，`geditor/` 是 legacy。
- `Render/GWMCP/` 是单独的 Python MCP 包，入口为 `python -m gw_mcp` 和 `render-engine-mcp`，要求 Python 3.11+。
- MCP server/bridge transport 的 contract 或 reference 文档变更，需要显式同步相关文档。
- 格式化规则：提交/推送前运行 `python scripts/Format.py -F`。

工作区路径：

```text
E:\code\GWCPEngine
```

## 2. Merge 提交内容梳理

用户问题：

```text
Merge branch 'sihui/GWMCP' into lzh/JIRA-GME-14125描述这次提交的内容
```

检查结果：

- 当前分支：`lzh/JIRA-GME-14125`
- 找到 merge commit：`d4e7425e17 Merge branch 'sihui/GWMCP' into lzh/JIRA-GME-14125`
- merge 父提交：
  - `a73cd44299`
  - `fb45f2f016`
- 被合入分支提交：`fb45f2f016 GWMCP code + opencode workflow`
- 规模：146 个文件，约 17170 行新增。

回答要点：

- 这次合入引入了 `Render/GWMCP` Python MCP Server 工程。
- 新增 Python tool 执行链路：`validators -> policy -> tcp bridge -> response mapper`。
- 新增 editor runtime C++ MCP bridge。
- Editor 启动时会创建本地 TCP listener，默认监听 `127.0.0.1:7009`。
- 主循环中每帧调用 `PumpRequests(5)`，确保 editor/engine 操作回到主线程执行。
- 新增 `.opencode/` 下的 MCP contract、workflow、agent、command 和上下文文档。

## 3. editor_runtime 中新增了什么

用户问题：

```text
editor\editro_runtime中新增了什么
```

回答要点：

- 理解路径为 `editor/editor/editor_runtime`。
- 新增一整套 `mcp/` 模块。
- 新增模块包括：
  - `EditorMcpBridge`
  - `protocol/`
  - `runtime/`
  - `dispatch/`
  - `handlers/`
  - `services/`
  - `docs/`
  - `types/`
- 修改已有文件：
  - `editor_runtime/CMakeLists.txt`
  - `system/Editor.cpp`
  - `system/Editor.h`
- 作用总结：新增的是 editor 侧 MCP 桥接层，让 Python MCP Server 可以通过本地 TCP 调用 editor runtime 的场景查询、组件编辑、资产加载和材质操作能力。

## 4. MCP 代码分步讲解

用户目标：

```text
分步详细讲解MCP部分的代码
```

讲解内容：

- 总体架构：

```text
MCP Client / AI 面板
  -> Python MCP Server
  -> TCP JSON-RPC
  -> C++ EditorMcpBridge
  -> Editor Runtime
  -> EngineCommandManager / Scene / Component / Asset
```

- Python Server 启动链路：

```text
python -m gw_mcp
  -> gw_mcp.__main__.main()
  -> cli.main()
  -> load_settings()
  -> create_server(settings)
  -> mcp.run(transport="stdio")
```

- Python tool 注册文件：
  - `Render/GWMCP/src/gw_mcp/tools/health.py`
  - `Render/GWMCP/src/gw_mcp/tools/scene_query.py`
  - `Render/GWMCP/src/gw_mcp/tools/scene_edit.py`

- Python `ToolRunner` 流程：

```text
validate -> policy -> tcp_client.send_request -> response_mapper
```

- C++ 侧启动：

```text
Editor::start()
  -> EditorMcpBridge::Start()
  -> RequestQueue
  -> BuildServices()
  -> BuildHandlers()
  -> BuildDispatcher()
  -> TcpServer::Start()
```

- 关键设计：
  - Python 是 MCP 门面和参数校验层。
  - C++ 是 editor/engine 执行层。
  - TCP server 线程不直接碰 editor 状态。
  - 所有真实 editor/engine 操作通过 `RequestQueue` 回到 editor 主线程执行。

## 5. MCP 作用和在引擎中的角色

用户问题：

```text
该MCP的作用是什么，在引擎中扮演什么角色
```

回答要点：

- GWMCP 是 AI/外部工具到 GritWorld Editor Runtime 的受控自动化接口。
- 它不是渲染模块，也不是 engine core，而是桥接/适配层。
- 能力包括：
  - 场景查询
  - 场景编辑
  - 资产和材质操作
  - 安全边界
  - 主线程调度
- 总结：

```text
MCP 是 AI 与 GritWorld Editor/Engine 交互的桥。
```

## 6. MCP 技术文档计划与落地

用户要求：

```text
为MCP写一份详细的技术文档，要求包含全流程，样例等内容
```

先提出计划，用户选择：

- 文档位置：根 `docs/`
- 详略程度：详细工程版

计划内容：

- 新增 `docs/mcp-technical-guide.md`
- 覆盖：
  - 当前实现基线
  - MCP 在引擎中的角色
  - 总体架构图
  - 启动链路
  - Python 全流程
  - C++ 全流程
  - 协议说明
  - Tool/Resource 清单
  - 端到端样例
  - 错误与安全边界
  - 调试与排错
  - 测试与验证

后续用户执行请求：

```text
PLEASE IMPLEMENT THIS PLAN
```

实际新增文件：

```text
docs/mcp-technical-guide.md
```

初始文档统计：

- 约 1000 行
- 中文详细工程版
- 覆盖 Python MCP Server、C++ Editor Bridge、TCP JSON-RPC、主线程调度、tools/resources、样例、错误、安全和测试。

## 7. 如何把 MCP 实现包装成产出

用户先问：

```text
如何包装成自己的产出
```

初始回答偏向“技术文档如何包装”。随后用户澄清：

```text
我是指如何把实现MCP这个包装成产出
```

建议包装为：

```text
GritWorld Editor MCP Bridge
GWMCP 编辑器自动化桥接能力
AI 驱动的 GritWorld Editor 操作接口
GritWorld Editor 智能化控制通道
```

一句话产出：

```text
完成 GritWorld Editor MCP 集成，实现 AI/外部工具对编辑器场景、组件、资产和材质的受控查询与编辑能力。
```

技术亮点：

- 打通 AI 到编辑器的调用链路。
- 提供可用的编辑器自动化能力。
- 保证操作安全可控。
- 建立跨语言桥接协议。
- 支撑后续扩展。

推荐述职写法：

```text
主导实现 GritWorld Editor MCP Bridge，构建 Python MCP Server + C++ Editor Runtime Bridge 的跨语言架构，基于 TCP JSON-RPC 打通 AI 工具到引擎编辑器的受控操作链路。设计 tool 注册、参数校验、安全策略、请求队列、主线程调度、handler/service 分发和统一错误映射机制，支持场景查询、组件编辑、资产加载和材质操作等能力，显著提升编辑器对 AI 自动化工作流的可接入性和可扩展性。
```

## 8. 文档补充：EditorMcpBridge::Start 每阶段做什么

用户要求：

```text
在文档中补充EditorMcpBridge::Start()
  -> RequestQueue
  -> BuildServices()
  -> BuildHandlers()
  -> BuildDispatcher()
  -> TcpServer::Start()每个阶段做了什么
```

修改文件：

```text
docs/mcp-technical-guide.md
```

新增说明：

- `RequestQueue`：跨线程请求队列，隔离 TCP server 线程和 editor 主线程。
- `BuildServices()`：创建 MCP 业务服务和 registry。
- `BuildHandlers()`：创建各类 handler 并注入 service。
- `BuildDispatcher()`：建立 `method -> handler` 路由表，检查重复 method。
- `TcpServer::Start()`：初始化并启动本地 TCP listener。

还补充了依赖关系图，说明 queue、registry、service、handler、dispatcher、tcp server 之间的装配顺序。

## 9. 文档补充：每个函数的作用

用户反馈：

```text
你应该把每个函数的作用都写上
```

修改文件：

```text
docs/mcp-technical-guide.md
```

新增函数职责表：

- `EditorMcpBridge`
  - 构造/析构
  - `Start`
  - `Stop`
  - `PumpRequests`
  - `IsRunning`
  - `ExportToolSchemas`
  - `InvokeSync`
  - `DispatchDirect`
  - `BuildServices`
  - `BuildHandlers`
  - `BuildDispatcher`

- `TcpServer`
  - 构造/析构
  - `Start`
  - `Stop`
  - `ServerThreadMain`
  - `HandleClient`

- `RequestQueue`
  - `PendingCall`
  - `Push`
  - `TryPop`
  - `DrainPending`

- `ToolDispatcher` / `IToolHandler`
  - 构造
  - `IsValid`
  - `Dispatch`
  - `SupportedMethods`
  - `Handle`

章节编号整理为：

```text
6.1 EditorMcpBridge 函数职责
6.2 TcpServer 函数职责
6.3 RequestQueue 与主线程调度
6.4 Dispatcher 与 Handler
6.5 Service 层
```

## 10. PendingCall 的作用

用户问题：

```text
pendingcall的作用是？
```

回答要点：

`PendingCall` 是一次“已经收到但还没在 editor 主线程执行完成”的 MCP 请求。它是 TCP 线程和 editor 主线程之间的同步载体。

结构：

```cpp
struct PendingCall {
    BridgeRequest request;
    BridgeResponse response;
    bool done{false};

    std::mutex mutex;
    std::condition_variable cv;
};
```

字段作用：

- `request`：保存 Python 发来的 JSON-RPC 请求解析结果。
- `response`：保存主线程执行完后的 JSON-RPC 响应。
- `done`：标记调用是否完成，避免正常完成、超时和关闭重复写结果。
- `mutex`：保护 `done` 和 `response`。
- `cv`：TCP 线程等待，主线程完成后唤醒。

一句话：

```text
PendingCall 是一次 MCP 请求的“待处理工单”，同时承载请求、响应、完成状态和跨线程唤醒机制。
```

## 11. EditorMcpBridge::InvokeSync 的作用

用户问题：

```text
editor::mcp::BridgeResult<nlohmann::json>
editor::mcp::EditorMcpBridge::InvokeSync(const std::string& method,
                                          const nlohmann::json& args,
                                          int timeoutMs)这个是？
```

回答要点：

- `InvokeSync` 是 Editor 内部调用 MCP tool 的同步入口。
- 它不是 Python 走 TCP 进来的路径。
- 它适合 C++/Editor 内部模块复用 MCP tool 语义。
- 它会创建 `PendingCall`，放入 `RequestQueue`，等待主线程 `PumpRequests()` 完成。
- 如果在 editor 主线程调用它，可能因为主线程无法同时 pump 队列而超时。
- 主线程上更适合用 `DispatchDirect(method, args)`。

对比：

```text
Python MCP Server
  -> TcpServer
  -> RequestQueue
  -> PumpRequests
  -> Dispatcher

C++ 内部调用方
  -> InvokeSync
  -> RequestQueue
  -> PumpRequests
  -> Dispatcher
```

## 12. 为什么使用 TCP

用户问题：

```text
为什么用tcp呢
```

回答要点：

- Python MCP Server 和 C++ Editor Runtime 是两个不同运行环境，需要跨语言 IPC。
- TCP 对 Python 和 C++ 都简单稳定。
- 两边进程解耦，Python 可独立启动、测试、打包。
- MCP Server 本身通常是独立进程，stdio 对 MCP Client，TCP 对 Editor。
- 监听地址是 `127.0.0.1:7009`，只接受本机连接。
- TCP + JSON-RPC 便于调试。
- 避免把 Python runtime 嵌进 Editor。
- 相比 HTTP/WebSocket 更轻。
- 相比共享内存更适合 JSON 小消息。

总结：

```text
用 TCP 是为了让 Python MCP Server 和 C++ Editor Bridge 低耦合、跨语言、可独立运行、可调试，并且通过本机 127.0.0.1 保持足够安全的进程间通信。
```

## 13. 为什么是 Python + C++，不直接用 C++

用户问题：

```text
为什么要python+c++，直接用c++不可以吗
```

回答要点：

- 理论上可以直接用 C++ 做 MCP Server。
- 当前选择 Python + C++ 是为了把“对外 MCP 协议层”和“引擎执行层”解耦。

Python 负责：

- MCP SDK / FastMCP 接入。
- tool/resource 注册。
- stdio transport。
- 参数校验。
- policy。
- response mapping。
- 快速测试和扩展。

C++ 负责：

- 接入 Editor Runtime。
- 回到 editor 主线程。
- 操作 scene/component/asset/material。
- 调用 `EngineCommandManager`。

总结：

```text
不是因为 C++ 做不了，而是 Python 更适合做 MCP 协议层和工具编排，C++ 更适合做 editor/engine 执行层。
```

## 14. Python 和 C++ 分别扮演的角色

用户问题：

```text
python和c++分别扮演的什么角色
```

回答要点：

Python 是“对外服务层/协议层”：

- 启动 MCP Server。
- 注册 MCP tools/resources。
- 接收 MCP Client 调用。
- 校验参数。
- 做安全策略。
- 转成 TCP JSON-RPC 发给 C++。
- 把结果包装成 MCP response。

C++ 是“引擎执行层/桥接层”：

- 启动本地 TCP listener。
- 接收 Python JSON-RPC 请求。
- 放入 `RequestQueue`。
- 主线程执行。
- 分发到 handler/service。
- 查询或修改 editor/engine 状态。

一句话：

```text
Python 是 MCP Server，负责对外沟通；C++ 是 Editor Bridge，负责对内执行。
```

## 15. types/ 下各个文件的作用

用户问题：

```text
types/下各个文件的作用
```

回答要点：

`editor/editor/editor_runtime/mcp/types/` 是 MCP 暴露给外部的类型/资产/材质白名单和映射规则。

文件：

```text
types/
  AssetRegistry.h
  AssetRegistry.cpp
  MaterialCreationRegistry.h
  McpTypeRegistry.h
  McpTypeRegistry.cpp
```

`AssetRegistry`：

- 定义 MCP 支持哪些资产类型。
- 定义扩展名、加载命令、是否支持查询。
- 当前支持：
  - `model`
  - `model_meta`
  - `material`

`MaterialCreationRegistry`：

- 定义 MCP 允许创建哪些材质类型。
- 当前支持：
  - `pbr`
  - `base`
  - `unlit`

`McpTypeRegistry`：

- 把 engine component 类型映射成 MCP type。
- 限制允许创建的 component 类型。
- 限制允许修改的 property。
- 导出 value types 和 supported component types。

总结：

```text
types/ 目录定义 MCP C++ bridge 对外暴露的“类型边界”：能识别什么 component、能创建什么 component、能改什么属性、能操作什么资产、能创建什么材质。
```

## 16. 什么是 MCP

用户问题：

```text
什么是mcp
```

回答要点：

MCP 是 `Model Context Protocol`，是一套让 AI 模型和外部工具/系统交互的标准协议。

核心概念：

- `MCP Client`：使用 MCP 能力的一方，例如 AI 面板、IDE 插件、Agent 客户端。
- `MCP Server`：提供能力的一方。
- `Tool`：可执行动作。
- `Resource`：可读取资料。
- `Prompt`：可复用提示模板。

放到 GWMCP 中：

```text
AI / MCP Client
  -> Python MCP Server
  -> C++ Editor Bridge
  -> GritWorld Editor / Engine
```

总结：

```text
MCP 是 AI 和 GritWorld Editor/Engine 之间的标准化操作接口。
```

## 17. Python 和 C++ 之间如何交流

用户问题：

```text
python和c++之间是怎么交流的
```

回答要点：

Python 和 C++ 之间通过本机 TCP + JSON-RPC 通信。

```text
Python gw_mcp
  -> TCP 127.0.0.1:7009
  -> C++ EditorMcpBridge
```

消息格式：

```text
[4 字节长度头][JSON-RPC 2.0 body]
```

C++ 接收后：

```text
FrameCodec::ReadFrame()
  -> JsonRpcProtocol::ParseRequest()
  -> 创建 PendingCall
  -> RequestQueue::Push(call)
  -> TCP 线程等待 call->cv
```

Editor 主线程执行：

```text
EditorMcpBridge::PumpRequests(5)
  -> RequestQueue::TryPop()
  -> ToolDispatcher::Dispatch()
  -> Handler
  -> Service
  -> EngineCommandManager / Editor / Scene / Component
```

## 18. JSON-RPC 说明

用户问题：

```text
json-rpc
```

回答要点：

JSON-RPC 是一种用 JSON 表达远程函数调用的协议。

请求示例：

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "set_transform",
  "params": {
    "request_id": "xxx",
    "client_name": "ai-panel",
    "session_id": null,
    "args": {
      "component_id": "component-guid",
      "position": [0, 1, 0],
      "rotation": null,
      "scale": null
    }
  }
}
```

成功响应：

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "component_id": "component-guid",
    "updated_fields": ["position"]
  }
}
```

失败响应：

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "error": {
    "code": "INVALID_ARGS",
    "message": "component_id must be a string",
    "details": {
      "field": "component_id"
    }
  }
}
```

额外说明：

- JSON-RPC 定义 JSON 中的 request/response 结构。
- GWMCP 还额外加了一层 4 字节 TCP frame length header。

## 19. 完整全链路例子

用户问题：

```text
举个完整的例子，全链路
```

例子：AI 调用 `set_transform`，把某个 component 移动到 `(0, 1, 0)`。

完整链路：

```text
AI / MCP Client
  -> Python MCP Server
  -> TcpBridgeClient
  -> TCP JSON-RPC
  -> C++ TcpServer
  -> RequestQueue
  -> Editor 主线程 PumpRequests
  -> ToolDispatcher
  -> TransformMutationHandler
  -> TransformService
  -> EngineCommandAdapter
  -> EngineCommandManager
  -> 返回结果
```

关键步骤：

1. AI 调用 MCP tool `set_transform(component_id, position)`。
2. Python `scene_edit.py` 接收调用并组装 `args`。
3. `ToolRunner` 执行 validator、policy、TCP bridge。
4. Python 打包 JSON-RPC 请求。
5. C++ `TcpServer` 读取 frame 并解析 request。
6. C++ 创建 `PendingCall` 放入 `RequestQueue`。
7. Editor 主线程 `PumpRequests()` 取出请求。
8. `ToolDispatcher` 找到 `TransformMutationHandler`。
9. `TransformMutationHandler` 解析参数。
10. `TransformService` 查找 component 并解析 Vector3。
11. `EngineCommandAdapter` 执行：

```text
Component SetPosition <component_id> 0.000000 1.000000 0.000000
```

12. C++ 生成 JSON-RPC success response。
13. TCP 线程写回 Python。
14. Python 解析 response。
15. `ResponseMapper` 包装成 MCP tool response 返回给 AI。

最终总结：

```text
AI 调 set_transform，Python MCP Server 做参数校验并发 JSON-RPC 到 C++，C++ TCP server 收到后把请求放入 RequestQueue，Editor 主线程 PumpRequests 取出请求并通过 ToolDispatcher -> TransformMutationHandler -> TransformService -> EngineCommandAdapter 执行 Component SetPosition，最后结果沿原路返回给 Python，再包装成 MCP response 给 AI。
```

## 20. 本轮产生和修改的文件

当前会话中明确由助手新增/修改的文档：

```text
docs/mcp-technical-guide.md
docs/mcp-conversation-export.md
```

当前工作区中还有一个未跟踪文件，出现于最后一次状态检查中，但不是本次导出操作创建：

```text
Render/GWMCP/docs/MCP_INTERVIEW_GUIDE.md
```

