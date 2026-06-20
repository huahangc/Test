# tqdm.h — C++ 终端进度条

一个受 Python [tqdm](https://github.com/tqdm/tqdm) 启发的 C++17 单头文件进度条库。

```
Processing |█████████████████████████████| 100% 200/200 [00:04<00:00, 49.44it/s]
Vec        |████████████████████████████| 100% 150/150 [00:03<00:00, 49.46it/s]
Range      |██████████████████████████████| 100% 100/100 [00:03<00:00, 33.15it/s]
```

## 特性

- **单头文件** — 只需 `#include "tqdm.h"`，无需链接任何库
- **三种使用方式** — 手动控制、容器遍历、整数范围
- **自动刷新节流** — 0.1 秒最小刷新间隔，高速迭代不会刷爆终端
- **自适应终端宽度** — 自动检测终端列数，动态调整进度条长度
- **完整信息显示** — 百分比、计数、已用时间、预估剩余时间 (ETA)、迭代速率
- **RAII 友好** — 析构时自动关闭进度条，不会破坏终端输出
- **无外部依赖** — 仅使用 C++17 标准库

## 快速开始

```cpp
#include "tqdm.h"
#include <vector>
#include <thread>

int main() {
    // 遍历容器
    std::vector<int> data(100);
    for (auto& x : tqdm::tqdm(data, "Processing")) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        x = 42;
    }

    // 整数范围
    for (auto i : tqdm::tqdm(50, "Counting")) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    return 0;
}
```

编译：

```bash
clang++ -std=c++17 -o main main.cpp
```

## 使用方式

### 1. 手动控制 ProgressBar

适合循环逻辑不规律、需要自定义步进的场景：

```cpp
tqdm::ProgressBar bar(1000, "Downloading");

for (int i = 0; i < 1000; ++i) {
    // 每次可以步进不同数量
    int step = some_function();
    bar.update(step);
}

bar.close();  // 输出最终状态并换行
// 即使忘记调用 close()，析构时也会自动关闭
```

**ProgressBar API：**

| 方法 | 说明 |
|------|------|
| `ProgressBar(total, prefix="", os=stderr)` | 构造，指定总数、可选前缀、可选输出流 |
| `update(n=1)` | 递增计数器 n 步，并刷新显示 |
| `set_prefix(prefix)` | 动态修改前缀文字 |
| `close()` | 强制刷新最终状态并换行 |
| `reset()` | 重置计数器和计时器，可复用 |
| `n()` | 当前迭代次数 |
| `total()` | 总迭代次数 |

### 2. 遍历容器

直接包装任何拥有 `.size()` 和 `.begin()`/`.end()` 的容器：

```cpp
std::vector<double> vec(200);
std::deque<int> dq(50);
std::string str(100, 'x');

for (auto& x : tqdm::tqdm(vec, "Vec"))  { /* ... */ }
for (auto& x : tqdm::tqdm(dq, "Deque")) { /* ... */ }
for (auto& c : tqdm::tqdm(str, "Str"))  { /* ... */ }
```

### 3. 整数范围

类似 Python 的 `for i in tqdm(range(100))`：

```cpp
// tqdm(count) — 从 0 到 count-1
for (auto i : tqdm::tqdm(100, "Epochs")) {
    // i = 0, 1, 2, ..., 99
}

// tqdm(start, stop) — 从 start 到 stop-1
for (auto i : tqdm::tqdm(50, 150, "Steps")) {
    // i = 50, 51, ..., 149
}
```

## 输出格式

```
prefix |████████████░░░░░░░░░░░|  45% 45/100 [00:05<00:06, 9.00it/s]
```

| 部分 | 说明 |
|------|------|
| `prefix` | 可选的描述文字前缀 |
| `\|████░░\|` | Unicode 可视化进度条（█ 填充，░ 空白） |
| `45%` | 完成百分比 |
| `45/100` | 当前迭代 / 总数 |
| `[00:05<00:06]` | 已用时间 < 预估剩余时间 (ETA) |
| `9.00it/s` | 平均迭代速率 |

时间格式为 `MM:SS`，超过 59 分 59 秒显示为 `59:59+`。

## 编译要求

- **C++17** 或更高
- **Linux / macOS**（使用 POSIX `ioctl` 检测终端宽度；其他平台回退到 `COLUMNS` 环境变量或默认 80 列）
- **UTF-8 终端**（进度条使用 Unicode 块字符）

## 完整示例

```cpp
#include "tqdm.h"
#include <iostream>
#include <thread>
#include <vector>
#include <cmath>

void demo_manual() {
    tqdm::ProgressBar bar(200, "Processing");
    for (int i = 0; i < 200; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        bar.update();
    }
    bar.close();
}

void demo_container() {
    std::vector<double> data(150);
    for (auto& x : tqdm::tqdm(data, "Vec")) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        x = std::sqrt(static_cast<double>(rand()));
    }
}

void demo_range() {
    for (auto i : tqdm::tqdm(100, "Range")) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

int main() {
    demo_manual();
    demo_container();
    demo_range();
    return 0;
}
```

## 注意事项

- 进度条输出到 `std::cerr`（与 Python tqdm 一致），不影响 `std::cout` 的管道操作
- 容器遍历时，`tqdm::tqdm()` 接受容器的**左值引用**，请确保容器在循环期间有效
- 该库**非线程安全**，如需在多线程中使用请自行加锁
