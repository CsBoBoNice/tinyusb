/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tusb.h"

//------------- FatFS -------------//
#include "ff.h"
#include "diskio.h"

#include <zephyr/drivers/disk.h>

#include <zephyr/fs/fs.h>
#include <zephyr/fs/fs_sys.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(tinyusb_msc, 1);

//---------------------------------//

//------------- FatFS -------------//
// for simplicity only support 1 LUN per device
// 为简单起见，每台设备只支持1个LUN
// static FATFS fatfs[CFG_TUH_DEVICE_MAX];
static volatile bool _disk_busy[CFG_TUH_DEVICE_MAX];
//---------------------------------//

//--------------------------------------------------------------------+
// DiskIO
//--------------------------------------------------------------------+

int usb_disk_status(struct disk_info *disk);
int usb_disk_initialize(struct disk_info *disk);
int usb_disk_read(struct disk_info *disk, uint8_t *data_buf,
		  uint32_t start_sector, uint32_t num_sector);
int usb_disk_write(struct disk_info *disk, const uint8_t *data_buf,
		   uint32_t start_sector, uint32_t num_sector);
int usb_disk_ioctl(struct disk_info *disk, uint8_t cmd, void *buff);

static int disk_usb_init(const struct device *dev); // 初始化USB磁盘

enum usb_status {
	USB_UNINIT,
	USB_ERROR,
	USB_OK,
};

struct usb_data {
	// struct sd_card card;
	enum usb_status status;
	char *name;

	// 用于标识驱动器的物理驱动器号
	BYTE pdrv;
};

struct usb_data tinyusb_data = {
	.name = "USB",
};

struct device usb_dev = {
	.name = "USB", // 设备实例的名称
	.config = NULL, // 设备实例配置信息
	.api = NULL, // 设备实例暴露的API结构
	.state = NULL, //通用设备状态
	.data = &tinyusb_data, // 设备实例私有数据
};

static const struct disk_operations sdmmc_disk_ops = {
	.init = usb_disk_initialize,
	.status = usb_disk_status,
	.read = usb_disk_read,
	.write = usb_disk_write,
	.ioctl = usb_disk_ioctl,
};

static struct disk_info usb_disk = {
	.ops = &sdmmc_disk_ops,
};
//--------------------------------------------------------------------+
// DiskIO
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// tinyusb
//--------------------------------------------------------------------+

CFG_TUH_MEM_ALIGN static scsi_inquiry_resp_t inquiry_resp;

//--------------------------------------------------------------------+
// tinyusb
//--------------------------------------------------------------------+

void msc_app_init(void)
{
	// 初始化USB磁盘
	int ret = disk_usb_init(&usb_dev);
	if (ret) {
		LOG_DBG("msc_app USB disk init failed %d \r\n", ret);
	} else {
		LOG_DBG("msc_app USB disk init success \r\n");
	}

	for (size_t i = 0; i < CFG_TUH_DEVICE_MAX; i++)
		_disk_busy[i] = false;
}

static FATFS fat_fs;
static struct fs_mount_t __mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
};
static struct fs_mount_t *mountpoint = &__mp;
static const char *disk_mount_pt = "/USB:";
static const char *disk_pdrv = "USB";

bool inquiry_complete_cb(uint8_t dev_addr,
			 tuh_msc_complete_data_t const *cb_data)
{
	msc_cbw_t const *cbw = cb_data->cbw;
	msc_csw_t const *csw = cb_data->csw;

	if (csw->status != 0) {
		LOG_DBG("msc_app Inquiry failed\r\n");
		return false;
	}

	// Print out Vendor ID, Product ID and Rev
	LOG_DBG("msc_app %.8s %.16s rev %.4s\r\n", inquiry_resp.vendor_id,
		inquiry_resp.product_id, inquiry_resp.product_rev);

	// Get capacity of device
	uint32_t const block_count =
		tuh_msc_get_block_count(dev_addr, cbw->lun);
	uint32_t const block_size = tuh_msc_get_block_size(dev_addr, cbw->lun);

	LOG_DBG("msc_app Disk Size: %" PRIu32 " MB\r\n",
		block_count / ((1024 * 1024) / block_size));
	LOG_DBG("msc_app Block Count = %" PRIu32 ", Block Size: %" PRIu32
		"\r\n",
		block_count, block_size);

	LOG_DBG("msc_app dev_addr = %d \r\n", dev_addr);

	mountpoint->storage_dev = (void *)disk_pdrv;
	mountpoint->mnt_point = disk_mount_pt;

	int ret = fs_mount(mountpoint);
	if (ret) {
		LOG_DBG("msc_app fs_mount failed: %d\r\n", ret);
	}

	return true;
}

//------------- IMPLEMENTATION -------------//
void tuh_msc_mount_cb(uint8_t dev_addr)
{
	LOG_DBG("msc_app A MassStorage device is mounted dev_addr %d \r\n",
		dev_addr);

	uint8_t const lun = 0;
	tuh_msc_inquiry(dev_addr, lun, &inquiry_resp, inquiry_complete_cb, 0);
}

void tuh_msc_umount_cb(uint8_t dev_addr)
{
	LOG_DBG("msc_app A MassStorage device is unmounted dev_addr %d \r\n",
		dev_addr);

	mountpoint->storage_dev = (void *)disk_pdrv;
	mountpoint->mnt_point = disk_mount_pt;

	int ret = fs_unmount(mountpoint);
	if (ret) {
		LOG_DBG("msc_app fs_unmount failed: %d\r\n", ret);
	}
}

//--------------------------------------------------------------------+
// DiskIO
//--------------------------------------------------------------------+

static void wait_for_disk_io(BYTE pdrv)
{
	LOG_DBG("msc_app wait_for_disk_io start pdrv=%d \r\n", pdrv);

	const char *thread_name;
	thread_name = k_thread_name_get(k_current_get());
	LOG_DBG("msc_app k_thread_name_get=%s \r\n", thread_name);

	while (_disk_busy[pdrv]) {
		tuh_task();
		// k_msleep(1);
		// tuh_task_ext(5000, false);
	}
	LOG_DBG("msc_app wait_for_disk_io end pdrv=%d \r\n\r\n", pdrv);
}

static bool disk_io_complete(uint8_t dev_addr,
			     tuh_msc_complete_data_t const *cb_data)
{
	(void)dev_addr;
	(void)cb_data;
	_disk_busy[dev_addr - 1] = false;

	LOG_DBG("msc_app disk_io_complete dev_addr %d \r\n", dev_addr);
	return true;
}

int usb_disk_status(struct disk_info *disk)
{
	struct usb_data *data = disk->dev->data;
	BYTE pdrv = data->pdrv;
	uint8_t dev_addr = pdrv + 1;

	LOG_DBG("msc_app usb_disk_status dev_addr=%d \r\n", dev_addr);

	return tuh_msc_mounted(dev_addr) ? 0 : STA_NODISK;
}

int usb_disk_initialize(struct disk_info *disk)
{
	struct usb_data *data = disk->dev->data;
	data->status = USB_OK;
	return 0;
}

/*
BYTE pdrv 标识驱动器的物理驱动器号 
BYTE *buff 存储读数据的数据缓冲区 
LBA_t sector 在LBA 中启动扇区
UINT count 要读的扇区数 
*/
int usb_disk_read(struct disk_info *disk, uint8_t *data_buf,
		  uint32_t start_sector, uint32_t num_sector)
{
	struct usb_data *data = disk->dev->data;

	BYTE pdrv = data->pdrv;
	BYTE *buff = data_buf;
	LBA_t sector = start_sector;
	UINT count = num_sector;

	uint8_t const dev_addr = pdrv + 1;
	uint8_t const lun = 0;

	LOG_DBG("msc_app usb_disk_read dev_addr=%d pdrv=%d \r\n", dev_addr,
		pdrv);

	_disk_busy[pdrv] = true;
	tuh_msc_read10(dev_addr, lun, buff, sector, (uint16_t)count,
		       disk_io_complete, 0);
	wait_for_disk_io(pdrv);

	return RES_OK;
}

int usb_disk_write(struct disk_info *disk, const uint8_t *data_buf,
		   uint32_t start_sector, uint32_t num_sector)
{
	struct usb_data *data = disk->dev->data;

	BYTE pdrv = data->pdrv;
	const BYTE *buff = data_buf;
	LBA_t sector = start_sector;
	UINT count = num_sector;

	uint8_t const dev_addr = pdrv + 1;
	uint8_t const lun = 0;

	LOG_DBG("msc_app usb_disk_write dev_addr=%d pdrv=%d \r\n", dev_addr,
		pdrv);

	_disk_busy[pdrv] = true;
	tuh_msc_write10(dev_addr, lun, buff, sector, (uint16_t)count,
			disk_io_complete, 0);
	wait_for_disk_io(pdrv);

	return RES_OK;
}

/*
BYTE pdrv;  // 物理驱动器号(0..) 
BYTE cmd;   // 控制代码
void *buff; // 发送/接收控制数据的缓冲区
*/
int usb_disk_ioctl(struct disk_info *disk, uint8_t cmd, void *buff)
{
	struct usb_data *data = disk->dev->data;

	BYTE pdrv = data->pdrv;

	uint8_t const dev_addr = pdrv + 1;
	uint8_t const lun = 0;

	LOG_DBG("msc_app usb_disk_ioctl dev_addr=%d pdrv=%d \r\n", dev_addr,
		pdrv);

	switch (cmd) {
	case CTRL_SYNC:
		// nothing to do since we do blocking
		// 由于我们采用阻塞方式，无需操作。
		LOG_DBG("msc_app usb_disk_ioctl CTRL_SYNC cmd=%d\n", cmd);
		return RES_OK;

	case GET_SECTOR_COUNT:
		*((DWORD *)buff) = (WORD)tuh_msc_get_block_count(dev_addr, lun);
		LOG_DBG("msc_app usb_disk_ioctl GET_SECTOR_COUNT cmd=%d\n",
			cmd);
		return RES_OK;

	case GET_SECTOR_SIZE:
		*((WORD *)buff) = (WORD)tuh_msc_get_block_size(dev_addr, lun);
		LOG_DBG("msc_app usb_disk_ioctl GET_SECTOR_SIZE cmd=%d\n", cmd);
		return RES_OK;

	case GET_BLOCK_SIZE:
		// erase block size in units of sector size
		// 以扇区大小为单位擦除块大小。
		*((DWORD *)buff) = 1;
		LOG_DBG("msc_app usb_disk_ioctl GET_BLOCK_SIZE cmd=%d\n", cmd);
		return RES_OK;

	case CTRL_LOCK:
		LOG_DBG("msc_app usb_disk_ioctl CTRL_LOCK cmd=%d\n", cmd);
		return RES_OK;

	case CTRL_EJECT:
		LOG_DBG("msc_app usb_disk_ioctl CTRL_EJECT cmd=%d\n", cmd);
		return RES_OK;

	case CTRL_POWER:
		LOG_DBG("msc_app usb_disk_ioctl CTRL_POWER cmd=%d\n", cmd);
		return RES_OK;

	default:
		LOG_DBG("msc_app usb_disk_ioctl ERROR cmd=%d\n", cmd);
		return RES_PARERR;
	}

	return RES_OK;
}

static int disk_usb_init(const struct device *dev)
{
	struct usb_data *data = dev->data;

	data->status = USB_UNINIT;
	usb_disk.dev = dev;
	usb_disk.name = data->name;

	return disk_access_register(&usb_disk);
}
