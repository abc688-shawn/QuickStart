# QuickStart

`QuickStart` 是一个 Unreal C++ 模块，用于驱动水下机器鱼仿真。它把鱼体运动、双目相机采集、IMU 模拟和 TCP 对接整合在一起，方便外部 Python 程序进行控制、接收图像和读取位姿。

这个仓库当前只包含模块源码，不包含完整 `.uproject`、关卡资源或模型资源。因此它更适合作为 `Source/QuickStart/` 目录直接接入现有 Unreal 工程，而不是独立开箱运行。

## 功能概览

- `ARotationActor`
  机器鱼主体 Actor，包含骨骼网格、双目相机、IMU、帧流组件和位姿遥测组件。
- `UFrameStreamingComponent`
  以约 `15 FPS` 将左右目渲染目标压缩为 JPEG，通过 TCP 发送给 Python。
- `UPoseTelemetryComponent`
  监听 TCP 连接，以约 `20 Hz` 向 Python 推送当前 Actor 位姿。
- `UIMUComponent`
  根据 Actor 的速度和姿态变化生成加速度、角速度和姿态估计，并叠加高斯噪声。
- `ATCPServer`
  监听控制端口，接收外部 `SetPose` 指令并驱动 `ARotationActor` 运动。

## 目录说明

- `RotationActor.*`: 机器鱼主体、双目相机、渲染目标和画中画逻辑
- `FrameStreamingComponent.*`: 双目图像 TCP 推流
- `PoseTelemetryComponent.*`: 位姿 TCP 遥测
- `IMUComponent.*`: IMU 模拟
- `TCPServer.*`, `TCPServerWorker.*`: 外部控制命令接入
- `RotationGameMode.*`: 游戏启动时创建 TCP 控制服务

## 依赖

- Unreal Engine C++ 工程
- Windows 环境
- OpenCV 4.9.0

`QuickStart.Build.cs` 里当前写死了本机 OpenCV 路径：

```csharp
string OpenCVPath = "C:\\Users\\lhs\\Downloads\\opencv\\build";
```

接入你自己的工程前，需要先把这个路径改成你机器上的实际 OpenCV 安装位置。

## 资源前提

代码里直接引用了以下 Unreal 资源路径：

```text
/Game/Robotfish/fish2_0/Geometries/SKM_fish2_0_Mesh
/Game/Robotfish/fish2_0/Geometries/All_SK_fish2_0_Mesh_Sequence1
```

如果这些资源不存在，`ARotationActor` 仍然会编译通过，但运行时不会正确显示鱼模型或动画。

另外，画中画 UI 依赖：

- 一个 `UUserWidget`，内部有名为 `PIPImage` 的 `Image`
- 一个用于 UI 显示的材质 `PIPCameraMaterial`

这两个资源需要在编辑器里给 `ARotationActor` 赋值。

## 接入方式

1. 将本目录放到 Unreal 工程的 `Source/QuickStart/`。
2. 确认 `.uproject` 已启用代码编译，并能找到 `OpenCV` 和 `OpenCVHelper`。
3. 修改 [QuickStart.Build.cs](/Users/shawn/UnderWaterSim/unreal/QuickStart/QuickStart.Build.cs:1) 中的 OpenCV 路径。
4. 编译工程。
5. 在关卡中放置一个 `ARotationActor`。
6. 将 GameMode 设置为 `ARotationGameMode`，启动时会自动拉起 TCP 控制服务。

## 网络接口

### 1. 外部位姿控制

`ATCPServer` 默认监听：

- `0.0.0.0:12345`

控制命令格式：

```text
SetPose:x,y,z,pitch,yaw,roll
```

示例：

```text
SetPose:100,200,150,0,90,0
```

说明：

- 位置单位是 `cm`
- 角度单位是 `degree`
- 当前实现里 `roll` 不参与最终 Actor 根节点旋转，内部会固定为 `0`

### 2. 双目图像推流

`UFrameStreamingComponent` 默认配置：

- Python 接收地址：`127.0.0.1:8989`
- Unreal 回收处理结果端口：`8990`
- 发送频率：约 `15 FPS`
- 渲染目标分辨率：`1920x1080`

TCP 帧格式为大端序：

```text
[4B left_size][left JPEG][4B right_size][right JPEG]
```

如果没有右目渲染目标，则只发送：

```text
[4B left_size][left JPEG]
```

当前代码会主动从 Unreal 侧连接 Python 的 `8989`，同时在 Unreal 本地监听 `8990`，等待 Python 回连。

### 3. 位姿遥测

`UPoseTelemetryComponent` 默认监听：

- `0.0.0.0:8991`

Python 连接成功后，Unreal 会按约 `20 Hz` 推送：

```text
Pose:x,y,z,pitch,yaw,roll
```

实际发送带换行：

```text
Pose:%.2f,%.2f,%.2f,%.4f,%.4f,%.4f\n
```

说明：

- 位置单位是 `cm`
- 姿态单位是 `degree`
- 注释中说明 Python 侧如需还原导航系 yaw，可在接收端自行加 `90°`

## IMU 输出

`UIMUComponent` 每帧更新以下量：

- `Acceleration`
- `AngularVelocity`
- `Orientation`

它会把 Unreal 坐标映射到鱼体坐标系，并叠加可配置的高斯噪声。默认噪声参数可直接在组件 Details 面板中调整。

## 运行提示

- `ARotationActor` 在 `BeginPlay` 中会创建左右目 `RenderTarget`
- 左右相机共用一个双目 rig，默认基线是 `3 cm`
- 左目实时画面会绑定到 PIP Widget
- `ARotationGameMode` 使用 `ASpectatorPawn` 作为默认 Pawn，避免默认球体进入相机视野

## 已知限制

- OpenCV 路径是硬编码的，本机路径不可直接移植
- 项目依赖外部 Unreal 资源，单独拷出源码无法完整复现效果
- 目前仓库里没有配套 Python 示例脚本
- `ATCPServer` 只缓存场景里找到的第一个 `ARotationActor`

## 后续建议

- 把 OpenCV 路径改成环境变量或工程配置
- 补一个最小 Python 客户端示例，覆盖 `12345`、`8989`、`8991` 三个接口
- 增加仓库根级 `.gitignore`、`.uproject` 和资源说明，便于他人复现
