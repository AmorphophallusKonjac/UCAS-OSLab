# Project 1 引导、镜像文件和ELF文件

## 实验任务

本实验包括五个任务，其中前三个为S-core，前四个为A-core，前五个为C-core.

具体如下：

- 任务1：第一个引导块的制作（C,A,S-core）
- 任务2：开始制作镜像文件（C,A,S-core）
- 任务3：加载并选择启动多个用户程序之一（C,A,S-core）
- 任务4：镜像文件的紧密排列（C,A-core）
- 任务5：内核镜像的压缩（C-core）

## 运行说明

```sh
make all        # 编译，制作镜像
make run        # 启动QEMU，模拟运行
# 进一步上板测试
make floppy     # 将镜像写入SD卡
make minicom    # 监视串口
```

进入BBL界面后，输入```loadboot```命令来启动bootloader。将进入以下界面：

```
=> loadboot
It's a bootloader...
Hello OS!
bss check: t version: 2
$ 
```

此时，操作系统已成功进入kernel，等待用户输入待执行的**应用（Application, 以下简称app）**

## 运作流程

### 引导块

```arch/riscv/boot/bootblock.S```完成下述操作：

- 打印```"It's bootblock!"```
- 将sd卡中的decompress解压程序所在块载入到内存0x52000000
- 将sd卡中的kernel所在块加载到内存0x54000000
- call decompress解压kernel到0x50201000
- 将sd卡中的task-info所在块加载到内存0x52000000
- 跳转到kernel入口地址

### 内核起点

```arch/riscv/kernel/head.S```完成下述操作：

- 清空bss段
- 设置内核栈指针

```init/main.c```完成下述操作：

- 检验bss段已清空
- 初始化bios
- 读取内存0x52000000处的```task-info```，保存到位于bss段的全局变量中，以防被后续应用覆盖
- 读取用户输入，调用各应用

### 用户任务的加载与执行

```kernel/loader/loader.c```共提供了两个函数，以供```init/main.c```调用：

```h
uint64_t load_task_img(int taskid);
void excute_task_img_via_name(char *taskname);
```

其中，

```load_task_img```将根据```taskid```从sd卡中读取下对应的task并跳转到该task的入口。

```from_name_load_task_img```将在```task-info```信息中逐个比对输入的task名。若输入的任务存在，则调用```load_task_img```函数执行该任务；否则，反馈失败信息给```init/main.c```。

### 测试程序入口

```crt0.S```完成下述操作：

- 清空bss段
- 保存返回地址到栈空间
- 调用main
- 恢复栈中的返回地址
- 返回内核

## 设计细节

### 镜像文件结构

镜像文件由```createimage.c```合成，具体依次存放了以下信息：

- bootblock (padding到512B)
- decompress
- kernel
- app 1, app 2, ... , app n
- task-info

其中，

在任务三中，没有decompress，kernel和各个app均被padding到15个扇区（512B）。

在任务四中，没有decompress，kernel，各个app以及app-info依次紧密存储。

在任务五中，decompress，kernel，各个app以及task-info依次紧密存储并且kernel是被压缩过的。

### task-info结构

task-info主要包括以下信息：

```h
// include/os/task.h

typedef struct {
    char name[32];
    int offset;
    int size;
    uint64_t entrypoint;
} task_info_t;

extern task_info_t tasks[TASK_MAXNUM];

// for [p1-task4]
extern uint16_t task_num;
```

其中，
- ```name```属性用来标识task的调用名
- ```offset```属性用来记录该task在image文件中距离文件开始位置的偏移量
- ```size```属性用来记录该task在image文件中的字节数
- ```entrypoint```属性来自ELF文件中记载的程序入口地址


### 辅助信息的存储

bios提供的```bios_sdread```函数需要提供块id，块数信息来一块一块地（512B）读取sd卡。这意味着需要存储kernel块数，task-info块id、块数信息，以辅助sd上数据块的加载。

注意到bootblock块末尾有大端空白，因此将这些信息存储在了bootblock块的最后的28B中。依次为：

- kernel在image文件中的偏移量，4B
- kernel块id，2B
- kernel块数，2B
- decompress大小，4B
- kernel大小，4B
- task-info在image文件中的偏移量，4B
- task-info大小，4B
- task-info块id，2B
- task-info块树，2B

这些信息随bootblock块在一开始就被加载到内存。

## 目录结构

```
Project1-BootLoader
├── arch
│   └── riscv
│       ├── bios
│       │   └── common.c
│       ├── boot
│       │   └── bootblock.S
│       ├── crt0
│       │   └── crt0.S
│       ├── decompress-crt0
│       │   └── decompress-crt0.S
│       ├── include
│       │   ├── asm
│       │   │   └── biosdef.h
│       │   ├── asm.h
│       │   ├── common.h
│       │   └── csr.h
│       └── kernel
│           └── head.S
├── build
├── createimage
├── include
│   ├── os
│   │   ├── kernel.h
│   │   ├── loader.h
│   │   ├── sched.h
│   │   ├── string.h
│   │   └── task.h
│   └── type.h
├── init
│   └── main.c
├── kernel
│   └── loader
│       └── loader.c
├── libs
│   └── string.c
├── Makefile
├── README.md
├── riscv.lds
├── test
│   └── test_project1
│       ├── 2048.c
│       ├── auipc.c
│       ├── bss.c
│       └── data.c
├── tiny_libc
│   └── include
│       └── kernel.h
└── tools
    ├── compress.c
    ├── createimage.c
    ├── decompress.c
    └── deflate
        ├── common_defs.h
        ├── example
        │   ├── Makefile
        │   └── test.c
        ├── lib
        │   ├── bt_matchfinder.h
        │   ├── decompress_template.h
        │   ├── deflate_compress.c
        │   ├── deflate_compress.h
        │   ├── deflate_constants.h
        │   ├── deflate_decompress.c
        │   ├── hc_matchfinder.h
        │   ├── ht_matchfinder.h
        │   ├── lib_common.h
        │   ├── matchfinder_common.h
        │   └── utils.c
        ├── libdeflate.h
        ├── stdbool.h
        ├── stddef.h
        ├── tinylibdeflate.c
        ├── tinylibdeflate.h
        └── type.h
```