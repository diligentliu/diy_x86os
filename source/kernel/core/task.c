#include "core/task.h"
#include "core/memory.h"
#include "core/syscall.h"
#include "cpu/cpu.h"
#include "cpu/irq.h"
#include "cpu/mmu.h"
#include "tools/klib.h"
#include "tools/log.h"
#include "comm/cpu_instr.h"
#include "comm/elf.h"
#include "fs/fs.h"
#include "os_cfg.h"

static task_manager_t task_manager;
static uint32_t idle_task_stack[IDLE_TASK_STACK_SIZE];
static task_t task_table[TASK_NR_MAX];
static mutex_t task_table_mutex;
static task_t *alloc_task();
static void free_task(task_t *task);

static int tss_init(task_t *task, int flag, uint32_t entry, uint32_t esp) {
	int tss_selector = gdt_alloc_desc();
	if (tss_selector < 0) {
		log_printf("alloc tss selector failed");
		return -1;
	}

	segment_desc_set(tss_selector, (uint32_t) &task->tss, sizeof(tss_t),
	                 SEG_P_PRESENT | SEG_DPL0 | SEG_TYPE_TSS);
	kernel_memset(&task->tss, 0, sizeof(tss_t));

	uint32_t kernel_stack = memory_alloc_page();
	if (kernel_stack == 0) {
		goto tss_init_failed;
	}

	int code_sel, data_sel;

	if (flag & TASK_FLAG_SYSTEM) {
		code_sel = KERNEL_SELECTOR_CS;
		data_sel = KERNEL_SELECTOR_DS;
	} else {
		code_sel = task_manager.app_code_selector | SEG_CPL3;
		data_sel = task_manager.app_data_selector | SEG_CPL3;
	}

	task->tss.eip = entry;
	task->tss.esp = esp;
	task->tss.esp0 = kernel_stack + MEM_PAGE_SIZE;
	task->tss.ss = data_sel;
	task->tss.ss0 = KERNEL_SELECTOR_DS;
	task->tss.es = task->tss.fs = task->tss.gs = task->tss.ds = data_sel;
	task->tss.cs = code_sel;
	task->tss.eflags = EFLAGS_IF | EFLAGS_DEFAULT;
	task->tss.iomap = 0;

	// 页表初始化
	uint32_t page_dir = memory_create_uvm();
	if (page_dir == 0) {
		goto tss_init_failed;
	}
	task->tss.cr3 = page_dir;

	task->tss_selector = tss_selector;
	return 0;
tss_init_failed:
	gdt_free_sel(tss_selector);

	if (kernel_stack) {
		memory_free_page(kernel_stack);
	}
	return -1;
}

/**
 * flag 0: 用户态
 * flag 1: 内核态
 */
int task_init(task_t *task, const char *name, int flag, uint32_t entry, uint32_t esp) {
	ASSERT(task != (task_t *) (0));

	tss_init(task, flag, entry, esp);

	kernel_strncpy(task->name, name, TASK_NAME_SIZE);
	task->state = TASK_CREATED;
	task->parent = (task_t *) 0;
	task->heap_start = task->heap_end = 0;
	task->sleep_ticks = 0;
	task->time_ticks = TASK_TIME_SLICE_DEFAULT;
	task->slice_ticks = TASK_TIME_SLICE_DEFAULT;
	task->status = 0;
	list_node_init(&task->run_node);
	list_node_init(&task->wait_node);
	list_node_init(&task->all_node);

	kernel_memset(&task->file_table, 0, sizeof(task->file_table));

	irq_state_t state = irq_enter_protection();
	task->pid = (uint32_t) task;

	list_push_back(&task_manager.task_list, &task->all_node);
	irq_leave_protection(state);
	return 0;
}

void task_start(task_t *task) {
	irq_state_t state = irq_enter_protection();
	task_set_ready(task);
	irq_leave_protection(state);
}

void task_uninit(task_t *task) {
	ASSERT(task != (task_t *) 0);
	ASSERT(task != &task_manager.idle_task);

	if (task->tss_selector) {
		gdt_free_sel(task->tss_selector);
	}

	if (task->tss.esp0) {
		memory_free_page(task->tss.esp0 - MEM_PAGE_SIZE);
	}

	if (task->tss.cr3) {
		memory_destroy_uvm(task->tss.cr3);
	}
	kernel_memset(task, 0, sizeof(task_t));
}

void task_switch_from_to(task_t *from, task_t *to) {
	switch_to_tss(to->tss_selector);
	// simple_switch(&from->stack, to->stack);
}

file_t *task_file(int fd) {
	task_t *task = task_current();
	if (fd < 0 || fd >= TASK_OFILE_NR) {
		return (file_t *) 0;
	}
	return task->file_table[fd];
}

int task_alloc_fd(file_t *file) {
	task_t *task = task_current();
	for (int i = 0; i < TASK_OFILE_NR; ++i) {
		if (task->file_table[i] == (file_t *) 0) {
			task->file_table[i] = file;
			return i;
		}
	}
	return -1;
}

void task_free_fd(int fd) {
	task_t *task = task_current();
	if (fd < 0 || fd >= TASK_OFILE_NR) {
		return;
	}
	task->file_table[fd] = (file_t *) 0;
}

static void idle_task_entry() {
	while (1) {
		hlt();
	}
}

void task_manager_init() {
	kernel_memset(task_table, 0, sizeof(task_table));
	mutex_init(&task_table_mutex);
	int sel = gdt_alloc_desc();
	segment_desc_set(sel, 0x00000000, 0xFFFFFFFF,
	                 SEG_P_PRESENT | SEG_DPL3 | SEG_S_NORMAL | SEG_TYPE_DATA | SEG_TYPE_RW | SEG_D
	);
	task_manager.app_data_selector = sel;

	sel = gdt_alloc_desc();
	segment_desc_set(sel, 0x00000000, 0xFFFFFFFF,
	                 SEG_P_PRESENT | SEG_DPL3 | SEG_S_NORMAL | SEG_TYPE_CODE | SEG_TYPE_RW | SEG_D
	);
	task_manager.app_code_selector = sel;

	task_manager.current = (task_t *) 0;
	list_init(&task_manager.sleep_list);
	list_init(&task_manager.ready_list);
	list_init(&task_manager.task_list);

	task_init(&task_manager.idle_task, "idle",
	          TASK_FLAG_SYSTEM,
	          (uint32_t) idle_task_entry,
	          (uint32_t) &idle_task_stack[IDLE_TASK_STACK_SIZE]);
	task_start(&task_manager.idle_task);
}

void first_task_init() {
	void first_task_entry();
	extern uint8_t s_first_task[], e_first_task[];

	uint32_t copy_size = (uint32_t) (e_first_task - s_first_task);
	uint32_t alloc_size = 10 * MEM_PAGE_SIZE;
	ASSERT(copy_size < alloc_size);

	uint32_t first_start = (uint32_t) first_task_entry;

	task_init(&task_manager.first_task,
	          "first task",
	          0,
	          first_start,
	          alloc_size + first_start);
	task_manager.first_task.heap_start = (uint32_t) e_first_task;
	task_manager.first_task.heap_end = (uint32_t) e_first_task;
	task_manager.current = &task_manager.first_task;

	mmu_set_page_dir(task_manager.first_task.tss.cr3);

	memory_alloc_page_for(first_start, alloc_size, PTE_P | PTE_W | PTE_U);
	kernel_memcpy((void *) first_start, (void *) s_first_task, copy_size);

	write_tr(task_manager.first_task.tss_selector);

	task_start(&task_manager.first_task);
}

task_t *task_first_task() {
	return &task_manager.first_task;
}

void task_set_ready(task_t *task) {
	if (task == &task_manager.idle_task) {
		return;
	}
	list_push_back(&task_manager.ready_list, &task->run_node);
	task->state = TASK_READY;
}

void task_set_block(task_t *task) {
	if (task == &task_manager.idle_task) {
		return;
	}
	list_ease(&task_manager.ready_list, &task->run_node);
}

task_t *task_current() {
	return task_manager.current;
}

// 返回下一个要运行的任务 (待优化)
task_t *task_next_run() {
	if (list_is_empty(&task_manager.ready_list)) {
		return &task_manager.idle_task;
	}
	list_node_t *task_node = list_first(&task_manager.ready_list);
	return list_node_parent(task_node, task_t, run_node);
}

int sys_yield() {
	irq_state_t state = irq_enter_protection();

	if (list_is_empty(&task_manager.ready_list)) {
		return -1;
	} else if (list_count(&task_manager.ready_list) == 1) {
		return 0;
	}

	task_t *current = task_current();
	task_set_block(current);
	task_set_ready(current);

	task_dispatch();

	irq_leave_protection(state);
	return 0;
}

void sys_exit(int status) {
	task_t *current = task_current();
	for (int fd = 0; fd < TASK_OFILE_NR; ++fd) {
		file_t *file = current->file_table[fd];
		if (file) {
			sys_close(fd);
			current->file_table[fd] = (file_t *) 0;
		}
	}

	int move_child = 0;

	// 找所有的子进程，将其转交给 init 进程
	mutex_lock(&task_table_mutex);
	for (int i = 0; i < TASK_NR_MAX; i++) {
		task_t *task = &task_table[i];
		if (task->parent == current) {
			// 有子进程，则转给init_task
			task->parent = &task_manager.first_task;

			// 如果子进程中有僵尸进程，唤醒回收资源
			// 并不由自己回收，因为自己将要退出
			if (task->state == TASK_ZOMBIE) {
				move_child = 1;
			}
		}
	}
	mutex_unlock(&task_table_mutex);

	irq_state_t state = irq_enter_protection();

	task_t *parent = current->parent;
	// 如果父进程为init进程，在下方唤醒
	if (move_child && parent != &task_manager.first_task) {
		if (task_manager.first_task.state == TASK_WAITTING) {
			task_set_ready(&task_manager.first_task);
		}
	}

	if (parent->state == TASK_WAITTING) {
		task_set_ready(parent);
	}

	current->status = status;
	current->state = TASK_ZOMBIE;
	task_set_block(current);
	task_dispatch();
	irq_leave_protection(state);
}

int sys_wait(int *status) {
	task_t *current = task_current();
	while (1) {
		mutex_lock(&task_table_mutex);

		for (int i = 0; i < TASK_NR_MAX; ++i) {
			task_t *task = &task_table[i];
			if (task->parent != current) {
				continue;
			}
			if (task->state == TASK_ZOMBIE) {
				int pid = task->pid;
				*status = task->status;
				memory_destroy_uvm(task->tss.cr3);
				memory_free_page(task->tss.esp0 - MEM_PAGE_SIZE);
				kernel_memset(task, 0, sizeof(task_t));

				mutex_unlock(&task_table_mutex);
				return pid;
			}
		}

		mutex_unlock(&task_table_mutex);

		irq_state_t state = irq_enter_protection();
		task_set_block(current);
		current->state = TASK_WAITTING;
		task_dispatch();
		irq_leave_protection(state);
	}
}

void task_dispatch() {
	irq_state_t state = irq_enter_protection();

	task_t *to = task_next_run();
	if (to == (task_t *) 0 || to == task_current()) {
		return;
	}
	task_t *from = task_current();
	task_manager.current = to;
	to->state = TASK_RUNNING;
	task_switch_from_to(from, to);

	irq_leave_protection(state);
}

void task_time_tick() {
	task_t *current = task_current();
	if (--current->slice_ticks <= 0) {
		current->slice_ticks = current->time_ticks;
		task_set_block(current);
		task_set_ready(current);
		task_dispatch();
	}

	list_node_t *node = list_first(&task_manager.sleep_list);
	while (node) {
		task_t *task = list_node_parent(node, task_t, run_node);
		list_node_t *next = list_node_next(node);
		if (--task->sleep_ticks <= 0) {
			task_set_wakeup(task);
			task_set_ready(task);
		}
		node = next;
	}
}

void task_set_sleep(task_t *task, uint32_t ticks) {
	if (ticks == 0) {
		return;
	}
	task->sleep_ticks = ticks;
	task->state = TASK_SLEEP;
	list_push_back(&task_manager.sleep_list, &task->run_node);
}

void task_set_wakeup(task_t *task) {
	list_ease(&task_manager.sleep_list, &task->run_node);
}

void sys_sleep(uint32_t ms) {
	irq_state_t state = irq_enter_protection();
	task_t *current = task_current();
	task_set_block(current);
	task_set_sleep(current, (ms + (OS_TICKS_MS - 1)) / OS_TICKS_MS);
	task_dispatch();
	irq_leave_protection(state);
}

int sys_getpid() {
	return task_current()->pid;
}

static void copy_opened_files(task_t *child) {
	task_t *parent = task_current();
	for (int i = 0; i < TASK_OFILE_NR; i++) {
		file_t *file = parent->file_table[i];
		if (file) {
			file_inc_ref(file);
			child->file_table[i] = file;
		}
	}
}

int sys_fork() {
	task_t *parent = task_current();
	task_t *child = alloc_task();
	if (child == (task_t *) 0) {
		goto fork_failed;
	}

	syscall_frame_t *frame = (syscall_frame_t *) (parent->tss.esp0 - sizeof(syscall_frame_t));
	int err = task_init(
			child,
			parent->name,
			0,
			frame->eip,
			frame->esp + sizeof(uint32_t) * SYSCALL_PARAM_COUNT
	);
	if (err < 0) {
		goto fork_failed;
	}

	copy_opened_files(child);

	tss_t *tss = &child->tss;
	tss->eax = 0;
	tss->ebx = frame->ebx;
	tss->ecx = frame->ecx;
	tss->edx = frame->edx;
	tss->esi = frame->esi;
	tss->edi = frame->edi;
	tss->ebp = frame->ebp;

	tss->cs = frame->cs;
	tss->ds = frame->ds;
	tss->es = frame->es;
	tss->fs = frame->fs;
	tss->gs = frame->gs;
	tss->eflags = frame->eflags;

	child->parent = parent;

	if ((tss->cr3 = memory_copy_uvm(parent->tss.cr3)) < 0) {
		goto fork_failed;
	}

	task_start(child);
	return child->pid;
fork_failed:
	if (child) {
		task_uninit(child);
		free_task(child);
	}
	return -1;
}

/**
 * @brief 加载一个程序表头的数据到内存中
 */
static int load_phdr(int file, Elf32_Phdr *phdr, uint32_t page_dir) {
	// 生成的ELF文件要求是页边界对齐的
	ASSERT((phdr->p_vaddr & (MEM_PAGE_SIZE - 1)) == 0);

	// 分配空间
	int err = memory_alloc_for_page_dir(page_dir, phdr->p_vaddr, phdr->p_memsz, PTE_P | PTE_U | PTE_W);
	if (err < 0) {
		log_printf("no memory");
		return -1;
	}

	// 调整当前的读写位置
	if (sys_lseek(file, phdr->p_offset, 0) < 0) {
		log_printf("read file failed");
		return -1;
	}

	// 为段分配所有的内存空间.后续操作如果失败，将在上层释放
	// 简单起见，设置成可写模式，也许可考虑根据phdr->flags设置成只读
	// 因为没有找到该值的详细定义，所以没有加上
	uint32_t vaddr = phdr->p_vaddr;
	uint32_t size = phdr->p_filesz;
	while (size > 0) {
		int curr_size = (size > MEM_PAGE_SIZE) ? MEM_PAGE_SIZE : size;

		uint32_t paddr = memory_get_paddr(page_dir, vaddr);

		// 注意，这里用的页表仍然是当前的
		if (sys_read(file, (char *) paddr, curr_size) < curr_size) {
			log_printf("read file failed");
			return -1;
		}

		size -= curr_size;
		vaddr += curr_size;
	}

	// bss区考虑由crt0和cstart自行清0，这样更简单一些
	// 如果在上边进行处理，需要考虑到有可能的跨页表填充数据，懒得写代码
	// 或者也可修改memory_alloc_for_page_dir，增加分配时清0页表，但这样开销较大
	// 所以，直接放在cstart哐crt0中直接内存填0，比较简单
	return 0;
}

/**
 * @brief 加载elf文件到内存中
 */
static uint32_t load_elf_file(task_t *task, const char *name, uint32_t page_dir) {
	Elf32_Ehdr elf_hdr;
	Elf32_Phdr elf_phdr;

	// 以只读方式打开
	int file = sys_open(name, 0);
	if (file < 0) {
		log_printf("load_elf_file: open file failed.%s", name);
		goto load_failed;
	}

	// 先读取文件头
	int cnt = sys_read(file, (char *) &elf_hdr, sizeof(Elf32_Ehdr));
	if (cnt < sizeof(Elf32_Ehdr)) {
		log_printf("load_elf_file: elf hdr too small. size=%d", cnt);
		goto load_failed;
	}

	// 做点必要性的检查。当然可以再做其它检查
	if ((elf_hdr.e_ident[0] != ELF_MAGIC) || (elf_hdr.e_ident[1] != 'E')
	    || (elf_hdr.e_ident[2] != 'L') || (elf_hdr.e_ident[3] != 'F')) {
		log_printf("load_elf_file: check elf indent failed.");
		goto load_failed;
	}

	// 必须是可执行文件和针对386处理器的类型，且有入口
	if ((elf_hdr.e_type != ET_EXEC) || (elf_hdr.e_machine != ET_386) || (elf_hdr.e_entry == 0)) {
		log_printf("load_elf_file: check elf type or entry failed.");
		goto load_failed;
	}

	// 必须有程序头部
	if ((elf_hdr.e_phentsize == 0) || (elf_hdr.e_phoff == 0)) {
		log_printf("load_elf_file: none programe header");
		goto load_failed;
	}

	// 然后从中加载程序头，将内容拷贝到相应的位置
	uint32_t e_phoff = elf_hdr.e_phoff;
	for (int i = 0; i < elf_hdr.e_phnum; i++, e_phoff += elf_hdr.e_phentsize) {
		if (sys_lseek(file, e_phoff, 0) < 0) {
			log_printf("load_elf_file: read file failed");
			goto load_failed;
		}

		// 读取程序头后解析，这里不用读取到新进程的页表中，因为只是临时使用下
		cnt = sys_read(file, (char *) &elf_phdr, sizeof(Elf32_Phdr));
		if (cnt < sizeof(Elf32_Phdr)) {
			log_printf("load_elf_file: read file failed");
			goto load_failed;
		}

		// 简单做一些检查，如有必要，可自行加更多
		// 主要判断是否是可加载的类型，并且要求加载的地址必须是用户空间
		if ((elf_phdr.p_type != PT_LOAD) || (elf_phdr.p_vaddr < MEMORY_TASK_BASE)) {
			continue;
		}

		// 加载当前程序头
		int err = load_phdr(file, &elf_phdr, page_dir);
		if (err < 0) {
			log_printf("load_elf_file: load program hdr failed");
			goto load_failed;
		}

		// 简单起见，不检查了，以最后的地址为bss的地址
		task->heap_start = elf_phdr.p_vaddr + elf_phdr.p_memsz;
		task->heap_end = task->heap_start;
	}

	sys_close(file);
	return elf_hdr.e_entry;

load_failed:
	if (file >= 0) {
		sys_close(file);
	}

	return 0;
}

/**
 * @brief 复制进程参数到栈中。注意argv和env指向的空间在另一个页表里
 */
static int copy_args(char *to, uint32_t page_dir, int argc, char **argv) {
	// 在stack_top中依次写入argc, argv指针，参数字符串
	task_args_t task_args;
	task_args.argc = argc;
	task_args.argv = (char **) (to + sizeof(task_args_t));

	// 复制各项参数, 跳过task_args和参数表
	// 各argv参数写入的内存空间
	char *dest_arg = to + sizeof(task_args_t) + sizeof(char *) * (argc + 1);   // 留出结束符

	// argv表
	char **dest_argv_tb = (char **) memory_get_paddr(page_dir, (uint32_t) (to + sizeof(task_args_t)));
	ASSERT(dest_argv_tb != 0);

	for (int i = 0; i < argc; i++) {
		char *from = argv[i];

		// 不能用kernel_strcpy，因为to和argv不在一个页表里
		int len = kernel_strlen(from) + 1;   // 包含结束符
		int err = memory_copy_uvm_data((uint32_t) dest_arg, page_dir, (uint32_t) from, len);
		ASSERT(err >= 0);

		// 关联ar
		dest_argv_tb[i] = dest_arg;

		// 记录下位置后，复制的位置前移
		dest_arg += len;
	}

	if (argc > 0) {
		dest_argv_tb[argc] = '\0';
	}
	// 写入task_args
	return memory_copy_uvm_data((uint32_t) to, page_dir, (uint32_t) &task_args, sizeof(task_args_t));
}

/**
 * @brief 加载一个进程
 * 这个比较复杂，argv/name/env都是原进程空间中的数据，execve中涉及到页表的切换
 * 在对argv和name进行处理时，会涉及到不同进程空间中数据的传递。
 */
int sys_execve(char *name, char **argv, char **env) {
	task_t *task = task_current();

	// 后面会切换页表，所以先处理需要从进程空间取数据的情况
	kernel_strncpy(task->name, get_file_name(name), TASK_NAME_SIZE);

	// 现在开始加载了，先准备应用页表，由于所有操作均在内核区中进行，所以可以直接先切换到新页表
	uint32_t old_page_dir = task->tss.cr3;
	uint32_t new_page_dir = memory_create_uvm();
	if (!new_page_dir) {
		goto exec_failed;
	}

	// 加载elf文件到内存中。要放在开启新页表之后，这样才能对相应的内存区域写
	uint32_t entry = load_elf_file(task, name, new_page_dir);    // 暂时置用task->name表示
	if (entry == 0) {
		goto exec_failed;
	}

	// 准备用户栈空间，预留环境环境及参数的空间
	uint32_t stack_top = MEM_TASK_STACK_TOP - MEM_TASK_ARG_SIZE;    // 预留一部分参数空间
	int err = memory_alloc_for_page_dir(new_page_dir,
	                                    MEM_TASK_STACK_TOP - MEM_TASK_STACK_SIZE,
	                                    MEM_TASK_STACK_SIZE, PTE_P | PTE_U | PTE_W);
	if (err < 0) {
		goto exec_failed;
	}

	// 复制参数，写入到栈顶的后边
	int argc = strings_count(argv);
	err = copy_args((char *) stack_top, new_page_dir, argc, argv);
	if (err < 0) {
		goto exec_failed;
	}

	// 加载完毕，为程序的执行做必要准备
	// 注意，exec的作用是替换掉当前进程，所以只要改变当前进程的执行流即可
	// 当该进程恢复运行时，像完全重新运行一样，所以用户栈要设置成初始模式
	// 运行地址要设备成整个程序的入口地址
	syscall_frame_t *frame = (syscall_frame_t *) (task->tss.esp0 - sizeof(syscall_frame_t));
	frame->eip = entry;
	frame->eax = frame->ebx = frame->ecx = frame->edx = 0;
	frame->esi = frame->edi = frame->ebp = 0;
	frame->eflags = EFLAGS_DEFAULT | EFLAGS_IF;  // 段寄存器无需修改

	// 内核栈不用设置，保持不变，后面调用memory_destroy_uvm并不会销毁内核栈的映射。
	// 但用户栈需要更改, 同样要加上调用门的参数压栈空间
	frame->esp = stack_top - sizeof(uint32_t) * SYSCALL_PARAM_COUNT;

	// 切换到新的页表
	task->tss.cr3 = new_page_dir;
	mmu_set_page_dir(new_page_dir);   // 切换至新的页表。由于不用访问原栈及数据，所以并无问题

	// 调整页表，切换成新的，同时释放掉之前的
	// 当前使用的是内核栈，而内核栈并未映射到进程地址空间中，所以下面的释放没有问题
	memory_destroy_uvm(old_page_dir);            // 再释放掉了原进程的内容空间

	// 当从系统调用中返回时，将切换至新进程的入口地址运行，并且进程能够获取参数
	// 注意，如果用户栈设置不当，可能导致返回后运行出现异常。可在gdb中使用nexti单步观察运行流程
	return 0;

exec_failed:    // 必要的资源释放
	if (new_page_dir) {
		// 有页表空间切换，切换至旧页表，销毁新页表
		task->tss.cr3 = old_page_dir;
		mmu_set_page_dir(old_page_dir);
		memory_destroy_uvm(new_page_dir);
	}

	return -1;
}

static task_t *alloc_task() {
	task_t *task = (task_t *) 0;

	mutex_lock(&task_table_mutex);
	for (int i = 0; i < TASK_NR_MAX; ++i) {
		task_t *t = &task_table[i];
		if (t->name[0] == '\0') {
			task = t;
			break;
		}
	}
	mutex_unlock(&task_table_mutex);
	return task;
}

static void free_task(task_t *task) {
	mutex_lock(&task_table_mutex);
	kernel_memset(task, 0, sizeof(task_t));
	mutex_unlock(&task_table_mutex);
}