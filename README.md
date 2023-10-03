# Project 2 简易内核实现

## 实验任务

本实验包括五个任务，其中前三个为S-core，前四个为A-core，前五个为C-core.

具体如下：

- 任务1：任务启动与非抢占式调度（C,A,S-core）
- 任务2：互斥锁的实现（C,A,S-core）
- 任务3：系统调用（C,A,S-core）
- 任务4：时钟中断、抢占式调度（C,A-core）
- 任务5：实现线程的创建 thread_create（C-core）

## 运行说明

```sh
make all        # 编译，制作镜像
make run        # 启动QEMU，模拟运行
# 进一步上板测试
make floppy     # 将镜像写入SD卡
make minicom    # 监视串口
```

进入BBL界面后，输入```loadbootd```命令来启动bootloader。将进入以下界面：

```
> [TASK] This task is to test scheduler. (227)                                  
> [TASK] This task is to test scheduler. (264)                                  
> [TASK] Has acquired lock and exited.                                          
> [TASK] Applying for a lock.                                                   
> [TASK] This task is to test sleep. (8)                                        
> [TASK] This is a thread to timing! (19/191143416 seconds).                    
                                                                                
                                                                                
                                                                                
                                                                                
                             _                                                 
                           -=\`\                                               
                       |\ ____\_\__                                             
                     -=\c`""""""" "`)                                           
                        `~~~~~/ /~~`                                            
                          -==/ /       
                            '-'                                          
```

此时，操作系统已成功进入kernel，并开始进程的调度。

## 运作流程

### 载入用户程序并初始化PCB

```init_pcb(init/main.c)```完成下述操作：
- 初始化PCB，包括pid、用户栈和内核栈
- 载入用户程序
- 将PCB加入内核栈

栈空间的分配详见```kernel/mm/mm.c```，代码中的栈空间分配如下：
- 内核栈空间：从0x50501000开始每次分配4096B
- 用户栈空间：从0x52500000开始每次分配4096B

比较tricky的是对于内核栈内容的初始化，模拟之前被调度放弃CPU之后存在内核栈上的上下文。这样第一次switch_to到用户程序的时候，就可以正常运行。

注意到```pcb[0]```被设定为```pid0_pcb```。我们认为其代表一个现在运行的进程，即将被调度。

### 初始化锁

```init_locks(kernel/locking/lock.c)```完成下述操作：
- 初始化锁，包括key、block队列和状态

### 初始化例外处理

```init_exception(kernel/irq/irq.c)```完成下述操作：
- 初始化例外入口表
- 初始化中断入口表
- 调用```setup_exception(arch/riscv/kernel/trap.S)```设置例外入口地址(```STVEC```)，打开全局中断。

### 初始化系统调用表

```init_syscall(init/main.c)```完成下述操作：
- 初始化系统调用表

### 初始化屏幕

```init_screen(drivers/screen.c)```完成下述操作：
- 初始化屏幕

### 开始调度

- S-core: 主动调用```do_scheduler(kernel/sched/sched.c)```开始调度
- C,A-core：设定下一次时钟中断时间，打开时钟中断使能，等待时钟中断开始调度

## 实现细节

### 调度算法

Round robin算法。每次调度将```ready_queue```队头进程替换当前正在运行的进程，调用```switch_to(arch/riscv/kernel/entry.S)```返回之后就是新的进程在运行了。

值得注意的是，执行```switch_to```的时候进程是运行在内核栈上的，而在任务3时才会把用户栈和内核栈区分开来。

### 互斥锁

初始化锁```do_mutex_lock_init(kernel/locking/lock.c)```，先找是否有相同```key```的锁，有就返回锁的编号，没有就找一个没有```key```的锁设置```key```并返回编号。

申请锁```do_mutex_lock_acquire(kernel/locking/lock.c)```，锁被锁上了就阻塞，直到锁没有锁上就占用锁并继续运行

释放锁```do_mutex_lock_release(kernel/locking/lock.c)```，释放锁，并且将锁的阻塞队列队头的进程加入```ready_queue```。

### 例外处理

发生例外后，进入S-Mode，跳转到```STVEC```也就是```exception_handler_entry(arch/riscv/kernel/entry.S)```，切换到内核栈，保存上下文到内核栈，跳转到```interrupt_helper(kernel/irq/irq.c)```处理例外，处理完毕之后跳转到```ret_from_exception(arch/riscv/kernel/entry.S)```，恢复上下文，切换到用户栈，用```sret```回到用户代码段。

### 睡眠

```do_sleep(kernel/sched/sched.c)```将当前进程加入全局阻塞队列，设定醒来时间，调度新的进程运行。

```check_sleeping(kernel/sched/time.c)```每次调度前先检查是否有进程要醒来了，将醒来的进程加入```ready_queue```
