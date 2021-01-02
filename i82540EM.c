#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/etherdevice.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/printk.h>

#include "i82540EM.h"
#include "debug_print.h"

MODULE_LICENSE("Dual BSD/GPL");

static const struct pci_device_id i82540EM_pci_tbl[] = {
	{PCI_DEVICE(i82540EM_VENDOR, i82540EM_DEVICE)},
	{}
};

static irqreturn_t test_isr(int irq, void* dev_id){

	struct i82540EM *i82540EM_dev = dev_id;
	int i = 0;

	u32 icr = readl(i82540EM_dev->regs + i82540EM_ICR);
	printk(KERN_INFO "INTERRUPT! ICR: 0x%11X\n", icr);

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
	printk(KERN_INFO "Tasklet rx_data(): RDH:RDT 0x%x:0x%x\n", head, tail);
	int i = 0;
	for(i = 0; i < i82540EM_SETTING_RX_BUFFER_COUNT; i++){
		printk(KERN_INFO "rx_data(): Descriptor[%d]: Length:%x Status:%x Checksum:%x Error:%x Special: %x Buffer DMA Address: %llx\n",
			i,
			i82540EM_dev->rx_descriptors[i].length,
			i82540EM_dev->rx_descriptors[i].status,
			i82540EM_dev->rx_descriptors[i].checksum,
			i82540EM_dev->rx_descriptors[i].errors,
			i82540EM_dev->rx_descriptors[i].special,
			*(void**)&i82540EM_dev->rx_descriptors[i]
		);
	}

uart_print("A\n\x00");

	// Process all done descriptors
	// Non-zero status means descriptor is done.
	while((tail + 1) % i82540EM_SETTING_RX_BUFFER_COUNT < head && i82540EM_dev->rx_descriptors[(tail + 1) % i82540EM_SETTING_RX_BUFFER_COUNT].status){

		// Increment the tail pointer here, so we can use it to point to the current descriptor we're processing.
		// This way we also avoid incrementing it later.
		tail++;
		tail %= i82540EM_SETTING_RX_BUFFER_COUNT;

		// Copy descriptor data to sk buffer.
		memcpy(skb_put(i82540EM_dev->packet_buffer,
			       i82540EM_dev->rx_descriptors[tail].length),
			i82540EM_dev->rx_buffers + (tail) * i82540EM_SETTING_RX_BUFFER_SIZE,
			i82540EM_dev->rx_descriptors[tail].length);

		printk(KERN_INFO "Copied to buffer\n");

		// If this was the last frame for the packet, send it up the networking stack.
		if(i82540EM_dev->rx_descriptors[tail].status & i82540EM_STATUS_BITMASK_EOP){

			printk(KERN_INFO "rx_data(): EOP detected\n");

			// Set metadata
			i82540EM_dev->packet_buffer->ip_summed = CHECKSUM_UNNECESSARY;
			i82540EM_dev->packet_buffer->dev = i82540EM_dev->net_dev;
			i82540EM_dev->packet_buffer->protocol = eth_type_trans(i82540EM_dev->packet_buffer, i82540EM_dev->net_dev);

uart_print("B\n\x00");

			// Print the packet to console.
			print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_NONE, 16, 1, i82540EM_dev->packet_buffer->data, i82540EM_dev->packet_buffer->data_len, true);

			// Send packet up the networking stack.
			//netif_rx(i82540EM_dev->packet_buffer);
			kfree_skb(i82540EM_dev->packet_buffer); // Debug

printk(KERN_INFO "C\n");

			// Allocate new packet buffer.
			// TODO: Critical bug here. This can fail. If it does, we need to
			// figure out how to handle the descriptors. We can drop them, but then
			// packets will be correupted until an EOP appears. Handle all of this well.
			i82540EM_dev->packet_buffer = dev_alloc_skb(i82540EM_SETTING_MAX_FRAME_SIZE);

		}
printk(KERN_INFO "D\n");

		// Set the status byte to zero so we know we handled this packet.
		i82540EM_dev->rx_descriptors[tail].status = 0;
printk(KERN_INFO "E\n");

		// Tail is already incremented for the next loop. Update the hw ring.
		// Barrier to ensure this happens only after status has been cleared.
		wmb();
		writel(tail, i82540EM_dev->regs + i82540EM_RDT);
printk(KERN_INFO "F\n");

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

	// Counter.
	unsigned int i = 0;

	printk("i82540EM: i82540EM_probe(): Device found. \n");

	uart_print("i82540EM: uart_print() works!\n\x00");

	// Attempt to enable the pci device
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

	// Enable bus mastering on the device
	pci_set_master(pci_dev);

	// Create the actual networking device with private
	// data of size 82540EM_dev
	net_dev = alloc_etherdev(sizeof(*i82540EM_dev));
	if(!net_dev){
		error = -ENOMEM;
		goto err_alloc_etherdev;
	}

	// Create sysfs symlink
	SET_NETDEV_DEV(net_dev, &pci_dev->dev);

	// Give the pci_dev a pointer to our net device
	pci_set_drvdata(pci_dev, net_dev);

	// The private data of the net device is our i82540EM drive object
	// Give it pointers back to it's parent net device and the PCI device.
	i82540EM_dev = netdev_priv(net_dev);
	i82540EM_dev->net_dev = net_dev;
	i82540EM_dev->pci_dev = pci_dev;

	// Initialize the RX tasklet.
	// Must be done before interrupts are enabled.
	tasklet_init(&i82540EM_dev->rx_tasklet, rx_data, i82540EM_dev);

	// Attempt to map the BAR registers
	i82540EM_dev->regs = pci_ioremap_bar(pci_dev, BAR_0);
	if(!i82540EM_dev->regs){
		dev_err(&pci_dev->dev, "Error mapping BAR 0 registers, exiting\n");
		error = -ENOMEM;
		goto err_pci_ioremap_bar;

	}

	// Initialize the spin lock.
	spin_lock_init(&i82540EM_dev->lock);

	// Reset the device.
	writel(i82540EM_CTRL_BITMASK_RST, i82540EM_dev->regs + i82540EM_CTRL);
	udelay(1000);

	// Wait for reset to complete.
	while(readl(i82540EM_dev->regs + i82540EM_CTRL) & i82540EM_CTRL_BITMASK_RST){
		printk(KERN_INFO "Waiting for reset bit to clear.\n");
		udelay(100);
	}

	// The ICS register isn't cleared on reset, unread pending interrupts here
	// will prevent new interrupts from being issued. So read the register to clear it.
	readl(i82540EM_dev->regs + i82540EM_ICR);

	// Allocate DMA mapping for the rx descriptor ring.
	// This should be on a 16-byte boundary. How do we ensure this?
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
	// Set ASDE & SLU
	// Clear PHY_RESET, ILOS, and VLAN
	u32 ctrl = readl(i82540EM_dev->regs + i82540EM_CTRL);
	ctrl |= (i82540EM_CTRL_BITMASK_ASDE & i82540EM_CTRL_BITMASK_SLU);
	ctrl &= ~(i82540EM_CTRL_BITMASK_PHY_RST | i82540EM_CTRL_BITMASK_ILOS | i82540EM_CTRL_BITMASK_VME);
	writel(ctrl, i82540EM_dev->regs + i82540EM_CTRL);

	// Initialize flow control registers to zero.
	writel(0, i82540EM_dev->regs + i82540EM_FCAL);
	writel(0, i82540EM_dev->regs + i82540EM_FCAH);
	writel(0, i82540EM_dev->regs + i82540EM_FCT);
	writel(0, i82540EM_dev->regs + i82540EM_FCTTV);

	// Program Ethernet Address.
	writel(0xCCDDEEFF, i82540EM_dev->regs + i82540EM_RAL);
	writel(0xAABB,     i82540EM_dev->regs + i82540EM_RAH);

	// Initialize the multicast table array.
	// 128 32-bit entries.
	for(i = 0; i < i82540EM_MTA_SIZE; i++){
		writel(0, i82540EM_dev->regs + i82540EM_MTA + (4 * i));
	}

	// Clear the interrupt mask.
	writel(0xFFFFFFFF, i82540EM_dev->regs + i82540EM_IMC);

	// Enable desired interrupts.
	writel(0xFFFFFFFF, i82540EM_dev->regs + i82540EM_IMS);
	//writel(i82540EM_INTERRUPT_BITMASK_RXT0, i82540EM_dev->regs + i82540EM_IMS);

	// Request the IRQ and register the handler here.
	printk(KERN_INFO "Requested IRQ\n");
	error = request_irq(pci_dev->irq, test_isr, IRQF_SHARED, "i82540EM", i82540EM_dev);
	if(error){
		dev_err(&pci_dev->dev, "Failed requesting IRQ, exiting.\n");
		goto err_request_irq;
	}
	printk(KERN_INFO "Got the IRQ\n");

	// Record that we accquired an IRQ, for releasing later.
	i82540EM_dev->irq_accquired = 1;

	// Initialize the rx descriptors.
	// Give each descriptor a buffer address in a cross platform way.
	// Initialize status to zero, just in case.
	for (i = 0; i < i82540EM_SETTING_RX_BUFFER_COUNT; i++){
		*(void**)(i82540EM_dev->rx_descriptors + i) = (void*)(i82540EM_dev->rx_buffers_dma_handle + i * i82540EM_SETTING_RX_BUFFER_SIZE);
		i82540EM_dev->rx_descriptors[i].status = 0;
	}

	// Give the controller the address of the rx descriptor ring.
	writel(i82540EM_dev->rx_descriptors_dma_handle & 0xFFFFFFFF, i82540EM_dev->regs + i82540EM_RDBAL);
	writel(i82540EM_dev->rx_descriptors_dma_handle >> 32, i82540EM_dev->regs + i82540EM_RDBAH);

	// Give the controller hte length of the rx descriptor ring in bytes.
	// Length must be a multiple of 128. WIth 16-byte descriptors, this means n*8 descriptors.
	writel(i82540EM_SETTING_RX_BUFFER_COUNT * i82540EM_RX_DESCRIPTOR_SIZE, i82540EM_dev->regs + i82540EM_RDLEN);

	// Initialize the head and tail pointers, which are both zero on reset.
	// RDH == RDT indicates empty ring, hw halts until new descriptors are added.
	// RDT == RDH-1 indicates a full ring, hardware can write to descriptors.
	// So initialize RDH to zero, and RDT to the last descriptor.
	writel(0, i82540EM_dev->regs + i82540EM_RDH);
	writel(i82540EM_SETTING_RX_BUFFER_COUNT-1, i82540EM_dev->regs + i82540EM_RDT);

	// Configure and enable the receiver.
	// This is the last step to start receiving packets.
	u32 rctl = readl(i82540EM_dev->regs + i82540EM_RCTL);
	rctl |= (i82540EM_RCTL_BITMASK_EN  |
		 i82540EM_RCTL_BITMASK_BAM |
		 i82540EM_RCTL_BITMASK_UPE |
		 i82540EM_RCTL_BITMASK_MPE );
	writel(rctl, i82540EM_dev->regs + i82540EM_RCTL);

	// Send test interrupt if desired.
	//printk(KERN_INFO "i82540EM: Sending test interrupt.\n");
	//writel(i82540EM_INTERRUPT_BITMASK_RXT0, i82540EM_dev->regs + i82540EM_ICS);

	// Done!
	printk(KERN_INFO "i82540EM: Init completed successfully.\n");

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

	printk("i82540EM: i82540EM_remove(): Removing device.\n");

	// Get the net_device assigned to this pci_dev device.
	struct net_device *net_dev = pci_get_drvdata(pci_dev);

	// If the pci device has an associated net device object, remove it.
	if (net_dev){

		// We want to unregister our net device here to stop
		// all activity before we do anything else.
		//unregister_netdev(net_dev);

		// Get the driver object from our net device.
		// This will always exist as it's part of the net_dev allocation.
		struct i82540EM *i82540EM_dev = netdev_priv(net_dev);

		// Free the IRQ if we had one.
		if(i82540EM_dev->irq_accquired)
			free_irq(pci_dev->irq, i82540EM_dev);

		// Free the rx descriptor ring buffer DMA mapping
		if(i82540EM_dev->rx_descriptors)
			dma_free_coherent(
				&i82540EM_dev->pci_dev->dev,
				i82540EM_SETTING_RX_BUFFER_COUNT * i82540EM_RX_DESCRIPTOR_SIZE,
				i82540EM_dev->rx_descriptors,
				i82540EM_dev->rx_descriptors_dma_handle);

		// Free the rx buffers DMA mapping
		if(i82540EM_dev->rx_buffers)
			dma_free_coherent(
				&i82540EM_dev->pci_dev->dev,
				i82540EM_SETTING_RX_BUFFER_COUNT * i82540EM_SETTING_RX_BUFFER_SIZE,
				i82540EM_dev->rx_buffers,
				i82540EM_dev->rx_buffers_dma_handle);

		// Unmap the registers if mapped.
		if(i82540EM_dev->regs)
			iounmap(i82540EM_dev->regs);

		// Free the net device and it's private data.
		// This erases our driver structure.
		free_netdev(net_dev);

		// Unmap resource associated with the pci device.
		pci_release_regions(pci_dev);

	}

//	if(pci_is_enabled(pci_dev))
//		pci_disable_device(pci_dev);


}

static struct pci_driver i82540EM_pci_driver = {
	.name 		= DRIVER_NAME,
	.id_table 	= i82540EM_pci_tbl,
	.probe		= i82540EM_probe,
	.remove		= i82540EM_remove,
};

module_pci_driver(i82540EM_pci_driver)

