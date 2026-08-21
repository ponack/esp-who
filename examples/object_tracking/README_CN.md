# 目标跟踪示例 [[English]](./README.md)

基于 ESP32-P4 Function EV Board 的触摸云台目标跟踪：MIPI CSI 摄像头接入人脸检测，ByteTrack 在帧间保持稳定 ID，在 LCD 上点击某张人脸即可选中目标，PID 驱动的两轴 pan/tilt 舵机云台会将该目标保持在画面中心。

本示例使用 `noglib` BSP：没有 LVGL，屏幕上也没有文字，只有彩色框（红色 = 跟踪中，绿色 + 十字准星 = 已选中）和触摸输入。

## 硬件

- ESP32-P4 Function EV Board（MIPI CSI 摄像头 + 800x1280 LCD + GT911 触摸）
- 两个 pan/tilt 舵机，例如 SG90、MG90S、MG996R。舵机信号脚和全部调参均可配置（见下文）。

## 交互

### 点击选中 / 取消目标

在 LCD（GT911）上点击。只有按下的上升沿算一次点击，命中检测使用当前的跟踪框：

- 点击某个人脸框，选中该目标。框变为绿色并带十字准星，云台开始将该人保持在画面中心。
- 再次点击已选中（绿色）的框，取消选中。云台保持最后的角度，等待下一次点击。
- 点击另一个人脸框，将目标切换到那个人。
- 点击空白区域没有任何效果。

串口命令 `home` 也会取消选中，并把两个舵机回到初始角度。

### 目标跟丢之后

若选中的人脸被遮挡，或走出画面：

- 云台**保持最后的位置**（不会自行回中）。
- 在一段宽限期内（约 30 帧，30 fps 时大约 1 秒）会保留同一个 track ID。若人脸及时重新出现，跟踪会自动恢复，无需再次点击。
- 若超过这段时间目标仍未出现，则释放选中。框变回红色，云台停在原地，直到你再点选一个新目标。

## 控制台命令

运行时调参通过串口控制台（`track>` 提示符）完成。参数由检测任务在下一帧边界应用，不会在帧中途生效。

| 命令 | 说明 |
| --- | --- |
| `help [command]` | 列出全部命令，或查看某条命令的帮助 |
| `pid <pan\|tilt> [kp\|ki\|kd <value>]` | 查看或设置 PID 增益 |
| `servo <pan\|tilt> [min\|max\|init\|dir <value>]` | 查看或设置舵机限位 / 初始角度 / 方向（`dir` 为 `1` 或 `-1`） |
| `cam [fx\|fy <value>]` | 查看或设置相机焦距 |
| `profile [list\|save\|load\|del] [name]` | 管理参数 profile（见下文） |
| `home` | 云台回到初始角度并取消选中 |

负值可用，例如 `servo pan dir -1`。

### 参数 Profile

调好的参数可以按名字保存到 NVS（最多 8 个），之后随时加载，重启后也仍然有效：

```
track> profile                # 当前 profile + dirty 标记 + 全部参数
track> profile list           # "*" 标记当前激活的 profile
track> profile save fast      # 将当前参数存为 profile "fast"
track> profile save           # 写回当前激活的 profile
track> profile load smooth    # 切换 profile
track> profile load default   # 回到 Kconfig 默认值
track> profile del fast       # 删除某个 profile（不能删当前激活的）
```

- `default` 是虚拟 profile：始终对应当前 Kconfig 的值，不能保存进去，也不能删除。
- 当前激活的 profile 会持久化，重启后自动恢复。
- `dirty` 表示当前参数与激活 profile 中保存的副本不一致。

## Kconfig

`idf.py menuconfig` → "Object Tracking Configuration"（见 `main/Kconfig.projbuild`）：

- **Pan/Tilt 舵机**：GPIO、方向、PID 增益、初始角度、角度范围
- **Camera**：焦距（fx、fy）

还需要根据开发板版本设置 **Channel for console output**。不同版本的 ESP32-P4 Function EV Board 可能使用不同的 console 硬件，请选择与硬件匹配的选项：

| 选项 | 硬件 |
| --- | --- |
| `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`（默认） | USB Serial/JTAG |
| `CONFIG_ESP_CONSOLE_UART_DEFAULT` | UART |
