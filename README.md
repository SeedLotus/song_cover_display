# 歌曲封面显示唱片机（Fork 优化版固件）

![cover](./Pic/封面2.jpg)

> 本仓库是 [TiX233/song_cover_display](https://github.com/TiX233/song_cover_display) 的 Fork，
> 聚焦 USB 通信协议可靠性与构建体系。项目原始介绍、硬件设计说明（主控 AT32F403ACCT7、
> GC9A01 圆形屏、磁保持唱臂、自研 ltx 调度器等）、烧录与 RTT 调试指引、调试命令列表，
> 请访问原仓库。
>
> **原作者**：realTiX（[realticks@qq.com](mailto:realticks@qq.com)）
> **原项目全量开源链接**：单片机 [GitHub](https://github.com/TiX233/song_cover_display) / [Gitee](https://gitee.com/TiX233/song_cover_display) ·
> 上位机 [GitHub](https://github.com/TiX233/song_cover_display_upper) / [Gitee](https://gitee.com/TiX233/song_cover_display_upper) ·
> [原理图/PCB（立创开源平台）](https://oshwhub.com/realtix/cover_display) ·
> [外壳（MakerWorld）](https://makerworld.com.cn/zh/models/1979900-dian-nao-ge-qu-feng-mian-xian-shi-chang-pian-ji)
>
> **授权**：个人学习用途免费，商用请联系作者获取授权。

---

## 本 Fork 的迭代优化

### 通信协议修复与扩展（协议 v2）

- **`pack_get_lost_counter` 循环变量未初始化修复**：原版首次图片传输误报丢包，导致重连后首张封面永不显示（上位机侧无法规避的固件缺陷）
- **TX 忙时回复不再静默丢弃**：`/k` `/a` `/r` `/x` 回复在 USB TX 忙时转入待重发缓存，下一周期自动重发。原版会静默丢弃——实测未烧录本修复的设备 `/a` **100% 丢失**
- **`/h` 心跳命令**：仅重置 60 秒休眠计时，不发布播放 topic、不碰显示。解决原协议矛盾：原版要求上位机定期发 `/1` `/0` 保活，但这会干扰摇臂状态机，不发又休眠。图片包接收同样计入活动
- **`/1` `/0` 幂等设态**：仅在播放状态真正翻转时发布摇臂 topic，重复收到同状态指令无副作用，彻底消除"丢一个包就摇臂反向/永久失同步"的整类问题；显示侧 `disp_pic_rotate` 保持无条件执行，`/1` 兼任封面传输后的转动恢复
- **`/v` 协议版本查询**：回复 `/v2`，上位机据此自动启用幂等设态行为；旧上位机与新固件、新上位机与旧固件均可安全混用
- **图片包序号越界写修复**：原版 `>` 比较放行 `index == 1888`，越界写 29 字节

### 构建体系

- **新增 GCC Makefile 构建**：无需 Keil MDK，`arm-none-eabi-gcc` 即可编译（原工程仅 Keil）
- **224K RAM 链接脚本**：芯片实际配置为 224K RAM 模式（96K 放不下 112.5KB 图片缓冲），使用项目内 `project/gcc/AT32F403ACCT7_FLASH.ld`，勿用库自带 96K 版

### 兼容性说明

| 上位机 \ 固件 | 旧固件（v1） | 新固件（v2） |
| - | - | - |
| 旧上位机 | 原始行为 | 行为不变（向后兼容） |
| 配套上位机（[song_cover_display_upper](https://github.com/SeedLotus/song_cover_display_upper) v1.2.0+） | 全部兼容：不依赖 `/k` `/a`，语义探测启用转动恢复 | `/v` 协商自动启用协议 v2：幂等设态、`/h` 保活不休眠、重连首封面可同步 |

---

## 构建

```bash
# MSYS2/mingw64 环境，需 arm-none-eabi-gcc 工具链
mingw32-make -j8
# 产物：build/firmware.hex / build/firmware.bin
```

Keil MDK 原始工程同样保留，可继续使用 Keil 编译。

## 烧录

SWD 调试器（J-Link / DAPLink 等）烧录 `build/firmware.hex`，或使用 Artery 官方 OpenOCD（含 `at32f403axx.cfg`）。详细烧录与 RTT 调试方法见原仓库 README。

## 注意事项

1. 开机时请确保唱臂运动不受阻碍，否则开机自检会判定唱臂异常，需重新插拔 USB 重启
2. 本项目含感性负载驱动，连接拓展坞需确保其带额外供电；直插电脑需确保 USB 插好，接触不良会导致唱臂无法正常驱动
