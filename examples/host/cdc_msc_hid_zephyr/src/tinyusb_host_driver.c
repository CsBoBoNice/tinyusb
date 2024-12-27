#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/sys/iterable_sections.h>

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/irq.h>

// LOG_MODULE_REGISTER(uhs, CONFIG_USBH_LOG_LEVEL);

#include "clock_config.h"
#include "pin_mux.h"

#include "tusb_config.h"

#include "usbh.h"

#include "fsl_device_registers.h"
#include "fsl_gpio.h"
#include "fsl_iomuxc.h"
#include "fsl_clock.h"

LOG_MODULE_REGISTER(tinyusb_host, 1);

void init_usb_phy(uint8_t usb_id)
{
	USBPHY_Type *usb_phy;

	if (usb_id == 0) {
		usb_phy = USBPHY1;
		CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_Usbphy480M,
					      BOARD_XTAL0_CLK_HZ);
		CLOCK_EnableUsbhs0Clock(kCLOCK_Usb480M, BOARD_XTAL0_CLK_HZ);
	}
#ifdef USBPHY2
	else if (usb_id == 1) {
		usb_phy = USBPHY2;
		CLOCK_EnableUsbhs1PhyPllClock(kCLOCK_Usbphy480M,
					      BOARD_XTAL0_CLK_HZ);
		CLOCK_EnableUsbhs1Clock(kCLOCK_Usb480M, BOARD_XTAL0_CLK_HZ);
	}
#endif
	else {
		return;
	}

	// Enable PHY support for Low speed device + LS via FS Hub
	usb_phy->CTRL |= USBPHY_CTRL_SET_ENUTMILEVEL2_MASK |
			 USBPHY_CTRL_SET_ENUTMILEVEL3_MASK;

	// Enable all power for normal operation
	// TODO may not be needed since it is called within CLOCK_EnableUsbhs0PhyPllClock()
	usb_phy->PWD = 0;

	// TX Timing
	uint32_t phytx = usb_phy->TX;
	phytx &= ~(USBPHY_TX_D_CAL_MASK | USBPHY_TX_TXCAL45DM_MASK |
		   USBPHY_TX_TXCAL45DP_MASK);
	phytx |= USBPHY_TX_D_CAL(0x0C) | USBPHY_TX_TXCAL45DP(0x06) |
		 USBPHY_TX_TXCAL45DM(0x06);
	usb_phy->TX = phytx;
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

void tuh_mount_cb(uint8_t dev_addr)
{
	// application set-up
	LOG_DBG("A device with address %d is mounted\r\n", dev_addr);
}

void tuh_umount_cb(uint8_t dev_addr)
{
	// application tear-down
	LOG_DBG("A device with address %d is unmounted \r\n", dev_addr);
}

extern void cdc_app_init(void);
extern void hid_app_init(void);
extern void msc_app_init(void);

// USB Host task
// This top level thread process all usb events and invoke callbacks
static void usb_host_task(void *arg1, void *arg2, void *arg3)
{
	// // init host stack on configured roothub port
	if (!tuh_init(BOARD_TUH_RHPORT)) {
		LOG_ERR("Failed to init USB Host Stack\r\n");
	}

	cdc_app_init();
	hid_app_init();
	msc_app_init();

	// RTOS forever loop
	while (1) {

		// put this thread to waiting state until there is new events
		tuh_task();
		// k_sleep(K_MSEC(10));

		// following code only run if tuh_task() process at least 1 event
	}
}

#define HOST_THREAD_TASK_SIZE 20480
static struct k_thread thread_usb_host_task_handler;
static k_thread_stack_t *usb_host_task_handler_stack;

void USB_HOST_business_init(void)
{
	// 创建线程
	usb_host_task_handler_stack =
		k_thread_stack_alloc(HOST_THREAD_TASK_SIZE, 0);
	if (usb_host_task_handler_stack == NULL) {
		LOG_ERR("alloc thread_usb_host_task_handler stack failed.");
	}
	k_tid_t thread = k_thread_create(&thread_usb_host_task_handler,
					 usb_host_task_handler_stack,
					 HOST_THREAD_TASK_SIZE,
					 (k_thread_entry_t)usb_host_task, NULL,
					 NULL, NULL, 3, 0, K_NO_WAIT);
	if (thread == NULL) {
		LOG_ERR("create thread_usb_host_task_handler failed.");
		k_thread_stack_free(usb_host_task_handler_stack);
		usb_host_task_handler_stack = NULL;
	}
	k_thread_name_set(&thread_usb_host_task_handler,
			  "thread_usb_host_task_handler");
}

//--------------------------------------------------------------------+
// USB Interrupt Handler
//--------------------------------------------------------------------+
static void USB_OTG1_IRQHandler(void)
{
	hcd_int_handler(0, true);
}

/*------------- USB_HOST_init -------------*/
int USB_HOST_init(void)
{
	IRQ_CONNECT(USB_OTG1_IRQn, IRQ_PRIO_LOWEST, USB_OTG1_IRQHandler, NULL,
		    0);

	//------------- USB -------------//
	// Note: RT105x RT106x and later have dual USB controllers.
	init_usb_phy(0); // USB0

	// #ifdef USBPHY2
	// 	init_usb_phy(1); // USB1
	// #endif

	USB_HOST_business_init();

	return 1;
}

SYS_INIT(USB_HOST_init, POST_KERNEL, 90);

#ifdef CONFIG_SHELL

#include <ctype.h>
#include <zephyr/shell/shell.h>
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <inttypes.h>

#define ROOT_PATH "/USB:/"

static char pwd_dir[128] = ROOT_PATH;

static int usb_msc_cat(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		return -1;
	}

	uint8_t buff[256] = { 0 };

	shell_print(sh, "path = %s", argv[1]);

	snprintf(buff, sizeof(buff), "%s%s", pwd_dir, argv[1]);

	FILE *fi = fopen(buff, "r");
	if (!fi) {
		shell_print(sh, "Error opening file");
		return -1;
	} else {
		shell_print(sh, "fopen success");
		size_t count = 0;
		size_t len = sizeof(buff) - 1;
		memset(buff, 0, sizeof(buff));
		while ((count = fread(buff, 1, len, fi)) > 0) {
			for (size_t c = 0; c < count; c++) {
				const uint8_t ch = buff[c];
				if (isprint(ch) || iscntrl(ch)) {
					// shell_print(sh, "%c", ch);
				} else {
					// shell_print(sh, ".");
					buff[c] = '.';
				}
			}

			shell_print(sh, "%s", buff);

			memset(buff, 0, sizeof(buff));
		}
	}

	fclose(fi); // 关闭文件

	return 0;
}

static int usb_msc_ls(const struct shell *sh, size_t argc, char **argv)
{
	if (argc > 1) {
		return -1;
	}

	const char *dpath = pwd_dir;

	DIR *dir;
	if (!(dir = opendir(dpath))) {
		shell_print(sh, "cannot access directory");
		return -1;
	}

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] != '.') {
			char path[128]; // 确保路径长度足够

			// snprintf(path, sizeof(path), "%s/%s", dpath,
			// 	 entry->d_name);
			// 以下代码脱裤子放屁 只是为了消除警告 替代以上简洁的代码

			char d_name_path[60] = { 0 };
			sscanf(entry->d_name, "%s", d_name_path);

			char pwd_dir_path[60] = { 0 };
			sscanf(dpath, "%s", pwd_dir_path);

			snprintf(path, sizeof(path), "%s/%s", pwd_dir_path,
				 d_name_path);

			struct stat file_stat;
			if (stat(path, &file_stat) == -1) {
				shell_print(sh, "cannot get file size");
			} else {
				mode_t mode = file_stat.st_mode;
				if (S_ISDIR(mode)) {
					shell_print(sh, "/%s\n", entry->d_name);
				} else {
					shell_print(sh, "%-40s", entry->d_name);
					size_t fsize = file_stat.st_size;
					if (fsize < 1024) {
						shell_print(sh, "%d B\n",
							    fsize);
					} else {
						shell_print(sh, "%d KB\n",
							    fsize / 1024);
					}
				}
			}
		}
	}

	closedir(dir);
	return 0;
}

static int usb_msc_pwd(const struct shell *sh, size_t argc, char **argv)
{
	if (argc > 1) {
		return -1;
	}

	shell_print(sh, "%s", pwd_dir);

	return 0;
}

static int usb_msc_cd(const struct shell *sh, size_t argc, char **argv)
{
	if (argc > 2) {
		return -1;
	}

	char path[128] = { 0 };

	if (argc == 1) {
		sprintf(pwd_dir, "%s", ROOT_PATH); // 更新当前目录
		shell_print(sh, "success cd %s", pwd_dir);
		return 1;
	} else {
		// snprintf(path, sizeof(path), "%s%s/", pwd_dir, argv[1]);

		// 以下代码脱裤子放屁 只是为了消除警告 替代以上简洁的代码

		char cd_path[60] = { 0 };
		sscanf(argv[1], "%s", cd_path);

		char pwd_dir_path[60] = { 0 };
		sscanf(pwd_dir, "%s", pwd_dir_path);

		sprintf(path, "%s%s/", pwd_dir_path, cd_path);
	}

	struct stat info;

	// 使用stat函数获取文件状态信息
	if (stat(path, &info) != 0) {
		shell_print(sh, "Error: checking folder existence %s", path);

		return -1;
	}

	// 检查是否为目录
	if (info.st_mode & S_IFDIR) {
		// 路径存在且为目录
		shell_print(sh, "success cd %s", path);
		sprintf(pwd_dir, "%s", path); // 更新当前目录
		return 1;
	} else {
		// 路径存在但不是目录
		shell_print(sh,
			    "Error: A path exists but is not a directory %s",
			    path);
		return 0;
	}

	return 0;
}

static int usb_msc_cp(const struct shell *sh, size_t argc, char **argv)
{
	if (argc > 3) {
		return -1;
	}

	char src[128] = { 0 };
	char dst[128] = { 0 };

	sprintf(src, "%s%s", pwd_dir, argv[1]);
	sprintf(dst, "%s%s", pwd_dir, argv[2]);

	// 使用标准C库函数打开源文件和目标文件
	FILE *f_src = fopen(src, "rb");
	FILE *f_dst = fopen(dst, "wb");

	if (f_src == NULL) {
		shell_print(sh,
			    "cannot open '%s': No such file or directory\r\n",
			    src);
		return -1;
	}

	if (f_dst == NULL) {
		shell_print(sh, "cannot create '%s'\r\n", dst);
		fclose(f_src);
		return -1;
	}

	// 缓冲区大小
	const size_t buf_size = 512;
	uint8_t buf[buf_size];
	size_t rd_count;

	// 读取源文件并写入目标文件
	while ((rd_count = fread(buf, 1, buf_size, f_src)) > 0) {
		size_t wr_count = fwrite(buf, 1, rd_count, f_dst);
		if (wr_count < rd_count) {
			shell_print(sh, "cannot write to '%s'\r\n", dst);
			break;
		}
	}

	// 关闭文件
	fclose(f_src);
	fclose(f_dst);

	shell_print(sh, "success copy %s to %s", src, dst);

	return 0;
}

static int usb_msc_rm(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		return -1;
	}

	uint8_t buff[128] = { 0 };

	snprintf(buff, sizeof(buff), "%s%s", pwd_dir, argv[1]);

	// 使用remove函数删除文件或目录
	if (remove(buff) != 0) {
		// 如果remove调用失败，打印错误信息
		shell_print(sh, "cannot remove '%s'", buff);
	} else {
		shell_print(sh, "success remove '%s'", buff);
	}

	return 0;
}

static int usb_msc_mv(const struct shell *sh, size_t argc, char **argv)
{
	if (argc > 3) {
		return -1;
	}

	char src[128] = { 0 };
	char dst[128] = { 0 };

	sprintf(src, "%s%s", pwd_dir, argv[1]);
	sprintf(dst, "%s%s", pwd_dir, argv[2]);

	// 使用rename函数重命名文件或目录
	if (rename(src, dst) != 0) {
		// 如果rename调用失败，打印错误信息
		shell_print(sh, "cannot mv '%s' to '%s'", src, dst);
		return -1;
	}

	shell_print(sh, "success mv '%s' to '%s'", src, dst);

	return 0;
}

static int usb_msc_mkdir(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		return -1;
	}

	uint8_t buff[128] = { 0 };

	snprintf(buff, sizeof(buff), "%s%s", pwd_dir, argv[1]);

	// 使用mkdir函数创建目录
	if (mkdir(buff, 0777) !=
	    0) // 第二个参数0777为创建目录的权限设置，可根据需要调整
	{
		// 如果mkdir调用失败，打印错误信息
		shell_print(sh, "cannot mkdir '%s'", buff);
	} else {
		// 如果mkdir调用成功，打印成功信息
		shell_print(sh, "success mkdir '%s'", buff);
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	mytest,
	SHELL_CMD_ARG(
		cat, NULL,
		"Usage: cat [FILE]...\r\n\tConcatenate FILE(s) to standard output..",
		usb_msc_cat, 2, 0),
	SHELL_CMD_ARG(
		ls, NULL,
		"Usage: ls [DIR]...\r\n\tList information about the FILEs (the current directory by default).",
		usb_msc_ls, 1, 0),
	SHELL_CMD_ARG(
		pwd, NULL,
		"Usage: pwd\r\n\tPrint the name of the current working directory.",
		usb_msc_pwd, 1, 0),
	SHELL_CMD_ARG(
		cd, NULL,
		"Usage: cd [DIR]...\r\n\tChange the current directory to DIR.",
		usb_msc_cd, 1, 1),
	SHELL_CMD_ARG(cp, NULL,
		      "Usage: cp SOURCE DEST\r\n\tCopy SOURCE to DEST.",
		      usb_msc_cp, 3, 0),
	SHELL_CMD_ARG(rm, NULL,
		      "Usage: rm [FILE]...\r\n\tRemove (unlink) the FILE(s).",
		      usb_msc_rm, 2, 0),
	SHELL_CMD_ARG(mv, NULL,
		      "Usage: mv SOURCE DEST...\r\n\tRename SOURCE to DEST.",
		      usb_msc_mv, 3, 0),
	SHELL_CMD_ARG(
		mkdir, NULL,
		"Usage: mkdir DIR...\r\n\tCreate the DIRECTORY(ies), if they do not already exist..",
		usb_msc_mkdir, 2, 0),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(usb_msc, &mytest, "Log test", NULL);

#endif
