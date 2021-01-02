#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/etherdevice.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>

#include "main.h"
#include "uart_print.h"

MODULE_LICENSE("Dual BSD/GPL");

static const struct pci_device_id i82540EM_pci_tbl[] = {
	{PCI_DEVICE(i82540EM_VENDOR, i82540EM_DEVICE)},
	{}
};

static irqreturn_t i82540EM_isr(int irq, void* dev_id){

	struct i82540EM *i82540EM_dev = dev_id;
	int i = 0;

	u32 icr = readl(i82540EM_dev->regs + i82540EM_ICR);
	uart_print("i82540EM_isr(): Interrupt cause: 0x%11X\n", icr);

	// Schedule the rx tasklet.
	// Multiple interrupts occuring may cause an ICR read to return zero.
	// This is fine, as long as the ISR that got a valid value handles
	// the cause.
	if(icr)
		tasklet_hi_schedule(&i82540EM_dev->rx_tasklet);

	return IRQ_RETVAL(1);
}

// Tasklet function to copy data from buffers.
// Serialized across CPUs.
void rx_data(long unsigned int param){

	struct i82540EM *i82540EM_dev = param;

	u32 head = readl(i82540EM_dev->regs + i82540EM_RDH);
	u32 tail = readl(i82540EM_dev->regs + i82540EM_RDT);

	// Debug. Dump all desriptors and head/tail information.
	uart_print("rx_data(): Tail: %d Head: %d\n", tail, head);
	int i = 0;
	for(i = 0; i < i82540EM_SETTING_RX_BUFFER_COUNT; i++){
		uart_print("rx_data(): Descriptor[%d]: Length:%x Status:%x Checksum:%x Error:%x Special: %x Buffer DMA Address: %llx\n",
			i,
			i82540EM_dev->rx_descriptors[i].length,
			i82540EM_dev->rx_descriptors[i].status,
			i82540EM_dev->rx_descriptors[i].checksum,
			i82540EM_dev->rx_descriptors[i].errors,
			i82540EM_dev->rx_descriptors[i].special,
			*(void**)&i82540EM_dev->rx_descriptors[i]
		);
	}

	// Process all done descriptors up to head.
	while((tail + 1) % i82540EM_SETTING_RX_BUFFER_COUNT != head && i82540EM_dev->rx_descriptors[(tail + 1) % i82540EM_SETTING_RX_BUFFER_COUNT].status){

		// Allocate new packet buffer if needed. If it fails, don't process descriptors.
		if (!i82540EM_dev->packet_buffer){

			i82540EM_dev->packet_buffer = dev_alloc_skb(i82540EM_SETTING_MAX_FRAME_SIZE);

			if(!i82540EM_dev->packet_buffer){
				uart_print("rx_data(): Failed to allocate packet buffer\n");
				return;
			}
		}

		// On interrupt, tail points to last processed descriptor.
		// Increment to point to the next valid descriptor.
		tail++;
		tail %= i82540EM_SETTING_RX_BUFFER_COUNT;

		// Copy descriptor data to sk buffer.
		memcpy(skb_put(i82540EM_dev->packet_buffer,i82540EM_dev->rx_descriptors[tail].length),
			i82540EM_dev->rx_buffers + (tail) * i82540EM_SETTING_RX_BUFFER_SIZE,
			i82540EM_dev->rx_descriptors[tail].length);

		// If end of packet, send it up the stack.
		if(i82540EM_dev->rx_descriptors[tail].status & i82540EM_STATUS_BITMASK_EOP){

			uart_print("rx_data(): EOP detected\n");

			// Set metadata
			i82540EM_dev->packet_buffer->ip_summed = CHECKSUM_UNNECESSARY;
			i82540EM_dev->packet_buffer->dev = i82540EM_dev->net_dev;
			i82540EM_dev->packet_buffer->protocol = eth_type_trans(i82540EM_dev->packet_buffer, i82540EM_dev->net_dev);

			// Print the packet to console.
			print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_NONE, 16, 1, i82540EM_dev->packet_buffer->data, i82540EM_dev->packet_buffer->data_len, true);

			// Send packet up the networking stack.
			//netif_rx(i82540EM_dev->packet_buffer);
			kfree_skb(i82540EM_dev->packet_buffer); // Debug

			// Remove our reference to the packet buffer, the kernel will free it.
			// Will be reallocated on next iteration.
			i82540EM_dev->packet_buffer = 0;

		}

		// Mark packet as handled.
		i82540EM_dev->rx_descriptors[tail].status = 0;

		// Tail is already incremented for the next loop. Update the hw ring.
		// Barrier to ensure this happens only after status has been cleared.
		wmb();
		writel(tail, i82540EM_dev->regs + i82540EM_RDT);

	}

}

static int i82540EM_probe(struct pci_dev *pci_dev, const struct pci_device_id *ent){

	/*
		pci_device ------------> Generic Device
		/\   |			    	/\
		|    | (pci_set/get_drvdata)	|
		|    \/			    	|
		| net_device		   	| (SET_NETDEV_DEV)
		|  /\ |			   	|
		|  |	 | (private data)    	|
		|  |	 \/		     	|
		(i82540EM driver object)--------/
	*/

	// Generic network device
	struct net_device *net_dev;

	// Driver-specific device object
	struct i82540EM *i82540EM_dev;

	// Return and init unwinding
	int error = 0;
	unsigned int i = 0;

	uart_print("i82540EM: i82540EM_probe(): Device found. \n");

	error = pci_enable_device(pci_dev);
	if(error){
		dev_err(&pci_dev->dev, "Failed enabling PCI debvice, exiting.\n");
		goto err_pci_enable_device;
	}

	// Request (reserve) the pci regions for the pci device from the kernel.
	error = pci_request_regions(pci_dev, DRIVER_NAME);
	if(error){
		dev_err(&pci_dev->dev, "Failed requesting PCI regions, exiting.\n");
		goto err_pci_request_regions;
	}

	pci_set_master(pci_dev);

	// Create the net device.
	net_dev = alloc_etherdev(sizeof(*i82540EM_dev));
	if(!net_dev){
		error = -ENOMEM;
		goto err_alloc_etherdev;
	}

	// Create sysfs symlink.
	SET_NETDEV_DEV(net_dev, &pci_dev->dev);

	// Give the pci_dev a pointer to our net device
	// Useful to access device object when pci device is being removed.
	pci_set_drvdata(pci_dev, net_dev);

	// The private data of the net device is our i82540EM drive object
	// Give it pointers back to it's parent net device and the PCI device.
	i82540EM_dev = netdev_priv(net_dev);
	i82540EM_dev->net_dev = net_dev;
	i82540EM_dev->pci_dev = pci_dev;

	// RX Tasklet must be initialized before interrupts are enabled.
	tasklet_init(&i82540EM_dev->rx_tasklet, rx_data, i82540EM_dev);

	i82540EM_dev->regs = pci_ioremap_bar(pci_dev, BAR_0);
	if(!i82540EM_dev->regs){
		dev_err(&pci_dev->dev, "Error mapping BAR 0 registers, exiting\n");
		error = -ENOMEM;
		goto err_pci_ioremap_bar;

	}

	spin_lock_init(&i82540EM_dev->lock);

	// Reset the device to bring all registers into default state.
	writel(i82540EM_CTRL_BITMASK_RST, i82540EM_dev->regs + i82540EM_CTRL);
	udelay(1000);

	// Documentation says reset requires a wait.
	while(readl(i82540EM_dev->regs + i82540EM_CTRL) & i82540EM_CTRL_BITMASK_RST){
		uart_print("Waiting for reset bit to clear.\n");
		udelay(100);
	}

	// Clear pending interrupts.
	readl(i82540EM_dev->regs + i82540EM_ICR);

	// Allocate DMA mapping for the rx descriptor ring.
	i82540EM_dev->rx_descriptors = dma_alloc_coherent(
					&pci_dev->dev,
					i82540EM_SETTING_RX_BUFFER_COUNT * i82540EM_RX_DESCRIPTOR_SIZE,
					&i82540EM_dev->rx_descriptors_dma_handle,
					GFP_KERNEL);

	if(!i82540EM_dev->rx_descriptors){
		dev_err(&pci_dev->dev, "Failed requesting DMA mapping for RX descriptor rings, exiting.\n");
		error = -ENOMEM;
		goto err_rx_descriptor_dma_alloc_coherent;
	}

	// Allocate DMA mapping for the receive buffers.
	i82540EM_dev->rx_buffers = dma_alloc_coherent(
				    &pci_dev->dev,
				    i82540EM_SETTING_RX_BUFFER_COUNT * i82540EM_SETTING_RX_BUFFER_SIZE,
				    &i82540EM_dev->rx_buffers_dma_handle,
			   	    GFP_KERNEL);

	if(!i82540EM_dev->rx_buffers){
		dev_err(&pci_dev->dev, "Failed requesting DMA mapping for RX buffers, exiting.\n");
		error = -ENOMEM;
		goto err_rx_buffers_dma_alloc_coherent;
	}

	// Program device control register as per documentation.
	u32 ctrl = readl(i82540EM_dev->regs + i82540EM_CTRL);
	ctrl |= (i82540EM_CTRL_BITMASK_ASDE & i82540EM_CTRL_BITMASK_SLU);
	ctrl &= ~(i82540EM_CTRL_BITMASK_PHY_RST | i82540EM_CTRL_BITMASK_ILOS | i82540EM_CTRL_BITMASK_VME);
	writel(ctrl, i82540EM_dev->regs + i82540EM_CTRL);

	// Initialize flow control registers to zero as per documentation.
	writel(0, i82540EM_dev->regs + i82540EM_FCAL);
	writel(0, i82540EM_dev->regs + i82540EM_FCAH);
	writel(0, i82540EM_dev->regs + i82540EM_FCT);
	writel(0, i82540EM_dev->regs + i82540EM_FCTTV);

	// Program Ethernet Address.
	writel(0xCCDDEEFF, i82540EM_dev->regs + i82540EM_RAL);
	writel(0xAABB,     i82540EM_dev->regs + i82540EM_RAH);

	// Initialize the multicast table array.
	for(i = 0; i < i82540EM_MTA_SIZE; i++){
		writel(0, i82540EM_dev->regs + i82540EM_MTA + (4 * i));
	}

	// Clear the interrupt mask.
	writel(0xFFFFFFFF, i82540EM_dev->regs + i82540EM_IMC);

	// Enable desired interrupts.
	writel(0xFFFFFFFF, i82540EM_dev->regs + i82540EM_IMS);
	//writel(i82540EM_INTERRUPT_BITMASK_RXT0, i82540EM_dev->regs + i82540EM_IMS);

	error = request_irq(pci_dev->irq, i82540EM_isr, IRQF_SHARED, "i82540EM", i82540EM_dev);
	if(error){
		dev_err(&pci_dev->dev, "Failed requesting IRQ, exiting.\n");
		goto err_request_irq;
	}

	// So we know to release the IRQ later.
	i82540EM_dev->irq_accquired = 1;

	// Initialize the rx descriptors.
	// Set status to zero, and provide the buffer address in a platform-independant way.
	for (i = 0; i < i82540EM_SETTING_RX_BUFFER_COUNT; i++){
		*(void**)(i82540EM_dev->rx_descriptors + i) = (void*)(i82540EM_dev->rx_buffers_dma_handle + i * i82540EM_SETTING_RX_BUFFER_SIZE);
		i82540EM_dev->rx_descriptors[i].status = 0;
	}

	// Supply the address of the rx descriptor ring.
	writel(i82540EM_dev->rx_descriptors_dma_handle & 0xFFFFFFFF, i82540EM_dev->regs + i82540EM_RDBAL);
	writel(i82540EM_dev->rx_descriptors_dma_handle >> 32, 	     i82540EM_dev->regs + i82540EM_RDBAH);

	// Supply the length of the rx descriptor ring. Must be a multiple of 128.
	writel(i82540EM_SETTING_RX_BUFFER_COUNT * i82540EM_RX_DESCRIPTOR_SIZE, i82540EM_dev->regs + i82540EM_RDLEN);

	// Initialize the head and tail pointers, which are both zero on reset.
	// RDH == RDT indicates empty ring, hw halts until new descriptors are added.
	// RDT == RDH - 1 indicates a full ring, hardware can write to descriptors.
	writel(0, i82540EM_dev->regs + i82540EM_RDH);
	writel(i82540EM_SETTING_RX_BUFFER_COUNT - 1, i82540EM_dev->regs + i82540EM_RDT);

	// Final step to receive packets, configure and enable the receiver.
	u32 rctl = readl(i82540EM_dev->regs + i82540EM_RCTL);
	rctl |= (i82540EM_RCTL_BITMASK_EN  | i82540EM_RCTL_BITMASK_BAM |
		 i82540EM_RCTL_BITMASK_UPE | i82540EM_RCTL_BITMASK_MPE );
	writel(rctl, i82540EM_dev->regs + i82540EM_RCTL);

	// Done!
	uart_print("i82540EM: Init completed successfully.\n");

	return 0;

err_request_irq:
	if(i82540EM_dev->rx_buffers)
		dma_free_coherent(
			&i82540EM_dev->pci_dev->dev,
			i82540EM_SETTING_RX_BUFFER_COUNT * i82540EM_SETTING_RX_BUFFER_SIZE,
			i82540EM_dev->rx_buffers,
			i82540EM_dev->rx_buffers_dma_handle);

err_rx_buffers_dma_alloc_coherent:
	if(i82540EM_dev->rx_descriptors)
		dma_free_coherent(
			&i82540EM_dev->pci_dev->dev,
			i82540EM_SETTING_RX_BUFFER_COUNT * i82540EM_RX_DESCRIPTOR_SIZE,
			i82540EM_dev->rx_descriptors,
			i82540EM_dev->rx_descriptors_dma_handle);

err_rx_descriptor_dma_alloc_coherent:
	iounmap(i82540EM_dev->regs);

err_pci_ioremap_bar:
	free_netdev(net_dev);

err_alloc_etherdev:
	pci_release_regions(pci_dev);

err_pci_request_regions:
	if(pci_is_enabled(pci_dev))
		pci_disable_device(pci_dev);

err_pci_enable_device:
	return error;
}

static void i82540EM_remove(struct pci_dev *pci_dev){

	uart_print("i82540EM: i82540EM_remove(): Removing device.\n");

	// Get the net_device assigned to this pci_dev device.
	struct net_device *net_dev = pci_get_drvdata(pci_dev);

	// If the pci device has an associated net device object, remove it.
	if (net_dev){

		// Unregister first to stop all activity.
		//unregister_netdev(net_dev);

		struct i82540EM *i82540EM_dev = netdev_priv(net_dev);

		if(i82540EM_dev->irq_accquired)
			free_irq(pci_dev->irq, i82540EM_dev);

		if(i82540EM_dev->rx_descriptors)
			dma_free_coherent(
				&i82540EM_dev->pci_dev->dev,
				i82540EM_SETTING_RX_BUFFER_COUNT * i82540EM_RX_DESCRIPTOR_SIZE,
				i82540EM_dev->rx_descriptors,
				i82540EM_dev->rx_descriptors_dma_handle);

		if(i82540EM_dev->rx_buffers)
			dma_free_coherent(
				&i82540EM_dev->pci_dev->dev,
				i82540EM_SETTING_RX_BUFFER_COUNT * i82540EM_SETTING_RX_BUFFER_SIZE,
				i82540EM_dev->rx_buffers,
				i82540EM_dev->rx_buffers_dma_handle);

		if(i82540EM_dev->regs)
			iounmap(i82540EM_dev->regs);

		// This erases our driver structure.
		free_netdev(net_dev);

		pci_release_regions(pci_dev);

	}

	if(pci_is_enabled(pci_dev))
		pci_disable_device(pci_dev);


}

static struct pci_driver i82540EM_pci_driver = {
	.name 		= DRIVER_NAME,
	.id_table 	= i82540EM_pci_tbl,
	.probe		= i82540EM_probe,
	.remove		= i82540EM_remove,
};

module_pci_driver(i82540EM_pci_driver)
