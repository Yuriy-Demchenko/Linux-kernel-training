#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/of.h>
#include <linux/of_device.h>



#define DRV_NAME  "plat_dummy"
/*Device has 2 resources:
* 1) 4K of memory at address defined by dts - used for data transfer;
* 2) Two 32-bit registers at address (defined by dts)
*  2.1. Flag Register: @offset 0
*	bit 0: PLAT_IO_DATA_READY - set to 1 if data from device ready
*	other bits: reserved;
* 2.2. Data size Register @offset 4: - Contain data size from device (0..4095);
*/
 #define MEM_SIZE	(4096)
#define REG_SIZE	(8)
#define DEVICE_POOLING_TIME_MS (1000) /*500 ms*/
/**/
#define PLAT_IO_FLAG_REG		(0) /*Offset of flag register*/
#define PLAT_IO_SIZE_REG		(4) /*Offset of size register*/
#define PLAT_IO_DATA_READY	(1) /*IO data ready flag */
#define MAX_DUMMY_PLAT_THREADS 1 /*Maximum amount of threads for this */


struct plat_dummy_device {
	void __iomem *mem_rx;
	void __iomem *regs_rx;
	void __iomem *mem_tx;
	void __iomem *regs_tx;
	struct delayed_work     dwork_rx;
	struct delayed_work     dwork_tx;
	struct workqueue_struct *data_read_wq;
	u64 js_pool_time;
};

static u32 plat_dummy_mem_read8(struct plat_dummy_device *my_dev, u32 offset)
{
	return ioread8(my_dev->mem_rx + offset);
}
static void plat_dummy_mem_write8(struct plat_dummy_device *my_dev, u32 offset, char val)
{
	iowrite8(val, my_dev->mem_tx + offset);
}

static u32 plat_dummy_reg_read32_rx(struct plat_dummy_device *my_dev, u32 offset)
{
	return ioread32(my_dev->regs_rx + offset);
}
static void plat_dummy_reg_write32_rx(struct plat_dummy_device *my_dev, u32 offset, u32 val)
{
	iowrite32(val, my_dev->regs_rx + offset);
}

static void plat_dummy_reg_write32_tx(struct plat_dummy_device *my_dev, u32 offset, u32 val)
{
	iowrite32(val, my_dev->regs_tx + offset);
}

static void plat_dummy_work_rx(struct work_struct *work)
{
	struct plat_dummy_device *my_device;
	u32 i, size, status;
	u8 data;

	pr_info("++%s(%d)\n", __func__, jiffies_to_msecs(jiffies));

	my_device = container_of(work, struct plat_dummy_device, dwork_rx.work);
	status = plat_dummy_reg_read32_rx(my_device, PLAT_IO_FLAG_REG);

	if (status & PLAT_IO_DATA_READY) {
		size = plat_dummy_reg_read32_rx(my_device, PLAT_IO_SIZE_REG);
		pr_info("%s: size = %d\n", __func__, size);

		if (size > MEM_SIZE)
			size = MEM_SIZE;

		for(i = 0; i < size; i++) {
			data = plat_dummy_mem_read8(my_device, i);
			pr_info("%s: mem[%d] = 0x%x ('%c')\n", __func__,  i, data, data);
		}
		rmb();
		status &= ~PLAT_IO_DATA_READY;
		plat_dummy_reg_write32_rx(my_device, PLAT_IO_FLAG_REG, status);
		queue_delayed_work(my_device->data_read_wq, &my_device->dwork_tx, my_device->js_pool_time);

	}
	queue_delayed_work(my_device->data_read_wq, &my_device->dwork_rx, my_device->js_pool_time);
	
}

static void plat_dummy_work_tx(struct work_struct *work)
{
	struct plat_dummy_device *my_device;
	u32 i;
	char buf[] = "Message from device received\n";

	pr_info("++%s(%d)\n", __func__, jiffies_to_msecs(jiffies));
	my_device = container_of(work, struct plat_dummy_device, dwork_tx.work);

	for (i = 0; i < strlen(buf); i++) {
		plat_dummy_mem_write8(my_device, i, buf[i]);
	}

	plat_dummy_reg_write32_tx(my_device, PLAT_IO_SIZE_REG, strlen(buf));
	plat_dummy_reg_write32_tx(my_device, PLAT_IO_FLAG_REG, PLAT_IO_DATA_READY);
}

static const struct of_device_id plat_dummy_of_match[] = {
	{
		.compatible = "test,plat_dummy",
	}, {
	},
 };

static int plat_dummy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct plat_dummy_device *my_device;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;

    pr_info("*HW* ++%s\n", __func__);

	if (!np) {
		pr_info("No device node found!\n");
		return -ENOMEM;
	}
	pr_info("Device name: %s\n", np->name);
	my_device = devm_kzalloc(dev, sizeof(struct plat_dummy_device), GFP_KERNEL);
	if (!my_device)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pr_info("res 0 = %llx..%llx\n", res->start, res->end);
	my_device->mem_rx = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->mem_rx))
		return PTR_ERR(my_device->mem_rx);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	pr_info("res 1 = %llx..%llx\n", res->start, res->end);
	my_device->regs_rx = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->regs_rx))
		return PTR_ERR(my_device->regs_rx);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	pr_info("res 2 = %llx..%llx\n", res->start, res->end);
	my_device->mem_tx = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->mem_tx))
		return PTR_ERR(my_device->mem_tx);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	pr_info("res 3 = %llx..%llx\n", res->start, res->end);
	my_device->regs_tx = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->regs_tx))
		return PTR_ERR(my_device->regs_tx);

	platform_set_drvdata(pdev, my_device);

	pr_info("Memory mapped to %p\n", my_device->mem_rx);
	pr_info("Registers mapped to %p\n", my_device->regs_rx);
	pr_info("Memory mapped to %p\n", my_device->mem_tx);
	pr_info("Registers mapped to %p\n", my_device->regs_tx);

	/*Init data read WQ*/
	my_device->data_read_wq = alloc_workqueue("plat_dummy_read",
					WQ_UNBOUND, MAX_DUMMY_PLAT_THREADS);

	if (!my_device->data_read_wq)
		return -ENOMEM;

	INIT_DELAYED_WORK(&my_device->dwork_rx, plat_dummy_work_rx);
	my_device->js_pool_time = msecs_to_jiffies(DEVICE_POOLING_TIME_MS);
	queue_delayed_work(my_device->data_read_wq, &my_device->dwork_rx, 0);

	INIT_DELAYED_WORK(&my_device->dwork_tx, plat_dummy_work_tx);
	my_device->js_pool_time = msecs_to_jiffies(DEVICE_POOLING_TIME_MS);

	return PTR_ERR_OR_ZERO(my_device->mem_rx);
}

static int plat_dummy_remove(struct platform_device *pdev)
{
	struct plat_dummy_device *my_device = platform_get_drvdata(pdev);

	pr_info("++%s\n", __func__);

	if (my_device->data_read_wq) {
	/* Destroy work Queue */
        cancel_delayed_work_sync(&my_device->dwork_rx);
        cancel_delayed_work_sync(&my_device->dwork_tx);
		destroy_workqueue(my_device->data_read_wq);
	}

        return 0;
}

static struct platform_driver plat_dummy_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = plat_dummy_of_match,
	},
	.probe		= plat_dummy_probe,
	.remove		= plat_dummy_remove,
};

MODULE_DEVICE_TABLE(of, plat_dummy_of_match);

module_platform_driver(plat_dummy_driver);

MODULE_AUTHOR("Vitaliy Vasylskyy <vitaliy.vasylskyy@globallogic.com>");
MODULE_DESCRIPTION("Dummy platform driver");
MODULE_LICENSE("GPL");
