#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/etherdevice.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>

#include "main.h"
#include "uart_print.h"

MODULE_LICENSE("Dual BSD/GPL");

static const struct net_device_ops i82540EM_net_ops;

static const struct pci_device_id i82540EM_pci_tbl[] = {
	{PCI_DEVICE(i82540EM_VENDOR, i82540EM_DEVICE)},
	{}
};

static irqreturn_t i82540EM_isr(int irq, void* dev_id){

	struct i82540EM *i82540EM_dev = dev_id;

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
void rx_data(unsigned long int param){

	struct i82540EM *i82540EM_dev = (struct i82540EM*)param;

	int i = 0;

	u32 head = readl(i82540EM_dev->regs + i82540EM_RDH);
	u32 tail = readl(i82540EM_dev->regs + i82540EM_RDT);

	// Debug. Dump all desriptors and head/tail information.
	uart_print("rx_data(): Tail: %d Head: %d\n", tail, head);
	for(i = 0; i < i82540EM_SETTING_RX_BUFFER_COUNT; i++){
		uart_print("rx_data(): Descriptor[%d]: Length:%d Status:%x Checksum:%x Error:%x Special: %x Buffer DMA Address: %llx\n",
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
		if (!i82540EM_dev->rx_skb_buffer){

			i82540EM_dev->rx_skb_buffer = dev_alloc_skb(i82540EM_SETTING_ETHERNET_MTU);
			if(!i82540EM_dev->rx_skb_buffer){
				uart_print("rx_data(): Failed to allocate packet buffer\n");
				return;
			}
		}

		// On interrupt, tail points to last processed descriptor.
		// Increment to point to the next valid descriptor.
		tail++;
		tail %= i82540EM_SETTING_RX_BUFFER_COUNT;

		uart_print("\n");
		buffer_uart_print(i82540EM_dev->rx_buffers + tail * i82540EM_SETTING_RX_BUFFER_SIZE, i82540EM_dev->rx_descriptors[tail].length, 16);
		uart_print("\n");

		// Copy data to packet buffer.
		memcpy(skb_put(i82540EM_dev->rx_skb_buffer, i82540EM_dev->rx_descriptors[tail].length),
			i82540EM_dev->rx_buffers + tail * i82540EM_SETTING_RX_BUFFER_SIZE,
			i82540EM_dev->rx_descriptors[tail].length);

		// If end of packet, send it up the stack.
		if(i82540EM_dev->rx_descriptors[tail].status & i82540EM_RX_STATUS_BITMASK_EOP){

			// Dump complete packet. Debug.
			uart_print("rx_data(): Dumping RX data.\n:");
			uart_print("rx_data(): rx_skb_buffer address: %llx\n", 			i82540EM_dev->rx_skb_buffer);
			uart_print("rx_data(): rx_skb_buffer->len: %d\n", 			i82540EM_dev->rx_skb_buffer->len);
			uart_print("rx_data(): rx_skb_buffer->head address: %llx\n", 		i82540EM_dev->rx_skb_buffer->head);
			uart_print("rx_data(): rx_skb_buffer->head physical address: %llx\n", 	virt_to_phys(i82540EM_dev->rx_skb_buffer->head));
			uart_print("rx_data(): RX packet bytes:\n");
			buffer_uart_print(i82540EM_dev->rx_skb_buffer->head, i82540EM_dev->rx_skb_buffer->len, 16);
			uart_print("\n");

			// Set metadata
			i82540EM_dev->rx_skb_buffer->ip_summed = CHECKSUM_UNNECESSARY;
			i82540EM_dev->rx_skb_buffer->dev = i82540EM_dev->net_dev;
			i82540EM_dev->rx_skb_buffer->protocol = eth_type_trans(i82540EM_dev->rx_skb_buffer, i82540EM_dev->net_dev);

			// Send packet up the networking stack.
			netif_rx(i82540EM_dev->rx_skb_buffer);

			// Remove our reference to the packet buffer, the kernel will free it.
			// Will be reallocated on next iteration.
			i82540EM_dev->rx_skb_buffer = 0;

		}

		// Mark packet as handled.
		i82540EM_dev->rx_descriptors[tail].status = 0;

		// Tail is already incremented for the next loop. Update the hw ring.
		// Barrier to ensure this happens only after status has been cleared.
		wmb();
		writel(tail, i82540EM_dev->regs + i82540EM_RDT);

	}

}

static void i82540EM_unmap_dma_mappings(struct i82540EM *i82540EM_dev){

	if(i82540EM_dev->rx_descriptors)
		dma_free_coherent(&i82540EM_dev->pci_dev->dev, i82540EM_SETTING_RX_BUFFER_COUNT * i82540EM_RX_DESCRIPTOR_SIZE, i82540EM_dev->rx_descriptors, i82540EM_dev->rx_descriptors_dma_handle);
	if(i82540EM_dev->tx_descriptors)
		dma_free_coherent(&i82540EM_dev->pci_dev->dev, i82540EM_SETTING_TX_BUFFER_COUNT * i82540EM_TX_DESCRIPTOR_SIZE, i82540EM_dev->tx_descriptors, i82540EM_dev->tx_descriptors_dma_handle);
	if(i82540EM_dev->rx_buffers)
		dma_free_coherent(&i82540EM_dev->pci_dev->dev, i82540EM_SETTING_RX_BUFFER_COUNT * i82540EM_SETTING_RX_BUFFER_SIZE, i82540EM_dev->rx_buffers, i82540EM_dev->rx_buffers_dma_handle);
	if(i82540EM_dev->tx_buffers)
		dma_free_coherent(&i82540EM_dev->pci_dev->dev, i82540EM_SETTING_TX_BUFFER_COUNT * i82540EM_SETTING_TX_BUFFER_SIZE, i82540EM_dev->tx_buffers, i82540EM_dev->tx_buffers_dma_handle);

	i82540EM_dev->rx_descriptors = 0;
	i82540EM_dev->tx_descriptors = 0;
	i82540EM_dev->rx_buffers = 0;
	i82540EM_dev->tx_buffers = 0;
}

static int i82540EM_init_dma_mappings(struct i82540EM *i82540EM_dev){

	// Allocate DMA mapping for the tx/rx descriptor rings and buffers.
	i82540EM_dev->rx_descriptors 	= dma_alloc_coherent(&i82540EM_dev->pci_dev->dev, i82540EM_SETTING_RX_BUFFER_COUNT * i82540EM_RX_DESCRIPTOR_SIZE, &i82540EM_dev->rx_descriptors_dma_handle, GFP_KERNEL);
	i82540EM_dev->tx_descriptors 	= dma_alloc_coherent(&i82540EM_dev->pci_dev->dev, i82540EM_SETTING_TX_BUFFER_COUNT * i82540EM_TX_DESCRIPTOR_SIZE, &i82540EM_dev->tx_descriptors_dma_handle, GFP_KERNEL);
	i82540EM_dev->rx_buffers 	= dma_alloc_coherent(&i82540EM_dev->pci_dev->dev, i82540EM_SETTING_RX_BUFFER_COUNT * i82540EM_SETTING_RX_BUFFER_SIZE, &i82540EM_dev->rx_buffers_dma_handle, GFP_KERNEL);
	i82540EM_dev->tx_buffers 	= dma_alloc_coherent(&i82540EM_dev->pci_dev->dev, i82540EM_SETTING_TX_BUFFER_COUNT * i82540EM_SETTING_TX_BUFFER_SIZE, &i82540EM_dev->tx_buffers_dma_handle, GFP_KERNEL);

	if(!i82540EM_dev->rx_descriptors || !i82540EM_dev->tx_descriptors || !i82540EM_dev->rx_buffers || !i82540EM_dev->tx_buffers){
		i82540EM_unmap_dma_mappings(i82540EM_dev);
		dev_err(&i82540EM_dev->pci_dev->dev, "Failed to allocate DMA mappings. Exiting.\n");
		return -ENOMEM;
	}

	return 1;
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

	// Pass the net device it's operation struct.
 	net_dev->netdev_ops = &i82540EM_net_ops;

	spin_lock_init(&i82540EM_dev->lock);

	// Map the BARs.
	i82540EM_dev->regs = pci_ioremap_bar(pci_dev, BAR_0);
	if(!i82540EM_dev->regs){
		dev_err(&pci_dev->dev, "Error mapping BAR 0 registers, exiting\n");
		error = -ENOMEM;
		goto err_pci_ioremap_bar;

	}

	// Reset the device to bring all registers into default state.
	// Wait loop needed as per doc.
	writel(i82540EM_CTRL_BITMASK_RST, i82540EM_dev->regs + i82540EM_CTRL);
	udelay(1000);
	while(readl(i82540EM_dev->regs + i82540EM_CTRL) & i82540EM_CTRL_BITMASK_RST){
		uart_print("Waiting for reset bit to clear.\n");
		udelay(100);
	}

	// Program device control registers.
	// We want:
	// ASDE
	// SLU
	// ~PHY_RST
	// ~ILOS
	// ~VME
	writel((i82540EM_CTRL_BITMASK_ASDE 	|
		i82540EM_CTRL_BITMASK_SLU)
		       	&
	      ~(i82540EM_CTRL_BITMASK_PHY_RST 	|
		i82540EM_CTRL_BITMASK_ILOS 	|
		i82540EM_CTRL_BITMASK_VME)	,
		i82540EM_dev->regs + i82540EM_CTRL);

	// Initialize flow control registers to zero as per documentation.
	writel(0, i82540EM_dev->regs + i82540EM_FCAL);
	writel(0, i82540EM_dev->regs + i82540EM_FCAH);
	writel(0, i82540EM_dev->regs + i82540EM_FCT);
	writel(0, i82540EM_dev->regs + i82540EM_FCTTV);

	// Program Ethernet Address.
	writel(ETHERNET_ADDRESS[0] << 8  | ETHERNET_ADDRESS[1], i82540EM_dev->regs + i82540EM_RAH);
	writel(ETHERNET_ADDRESS[2] << 24 | ETHERNET_ADDRESS[3] << 16 | ETHERNET_ADDRESS[4] << 8 | ETHERNET_ADDRESS[5], i82540EM_dev->regs + i82540EM_RAL);
	net_dev->dev_addr[0] = ETHERNET_ADDRESS[0];
	net_dev->dev_addr[1] = ETHERNET_ADDRESS[1];
	net_dev->dev_addr[2] = ETHERNET_ADDRESS[2];
	net_dev->dev_addr[3] = ETHERNET_ADDRESS[3];
	net_dev->dev_addr[4] = ETHERNET_ADDRESS[4];
	net_dev->dev_addr[5] = ETHERNET_ADDRESS[5];

	// Initialize the multicast table array.
	for(i = 0; i < i82540EM_MTA_SIZE; i++)
		writel(0, i82540EM_dev->regs + i82540EM_MTA + (4 * i));

	// Request the DMA mappings for the RX/TX descriptors and buffers.
	error = i82540EM_init_dma_mappings(i82540EM_dev);
	if(error){
		dev_err(&pci_dev->dev, "Error requesting DMA mappings, exiting.\n");
		goto err_init_dma_mappings;
	}

	// Initialize the RX/TX descriptors.
	// Set status to zero, and provide the buffer address in a platform-independant way.
	for (i = 0; i < i82540EM_SETTING_RX_BUFFER_COUNT; i++){
		*(void**)(i82540EM_dev->rx_descriptors + i) = (void*)(i82540EM_dev->rx_buffers_dma_handle + i * i82540EM_SETTING_RX_BUFFER_SIZE);
		i82540EM_dev->rx_descriptors[i].status = 0;
	}
	for (i = 0; i < i82540EM_SETTING_TX_BUFFER_COUNT; i++){
		*(void**)(i82540EM_dev->tx_descriptors + i) = (void*)(i82540EM_dev->tx_buffers_dma_handle + i * i82540EM_SETTING_TX_BUFFER_SIZE);
		i82540EM_dev->tx_descriptors[i].status = 0;
	}

	// Write addresses of RX/TX descriptor rings.
	writel(i82540EM_dev->rx_descriptors_dma_handle >> 32, 	     i82540EM_dev->regs + i82540EM_RDBAH);
	writel(i82540EM_dev->rx_descriptors_dma_handle & 0xFFFFFFFF, i82540EM_dev->regs + i82540EM_RDBAL);
	writel(i82540EM_dev->tx_descriptors_dma_handle >> 32, 	     i82540EM_dev->regs + i82540EM_TDBAH);
	writel(i82540EM_dev->tx_descriptors_dma_handle & 0xFFFFFFFF, i82540EM_dev->regs + i82540EM_TDBAL);

	// Write length of RX/TX descriptor rings.
	writel(i82540EM_SETTING_RX_BUFFER_COUNT * i82540EM_RX_DESCRIPTOR_SIZE, i82540EM_dev->regs + i82540EM_RDLEN);
	writel(i82540EM_SETTING_TX_BUFFER_COUNT * i82540EM_TX_DESCRIPTOR_SIZE, i82540EM_dev->regs + i82540EM_TDLEN);

	// Initialize head and tail offsets for RX/TX descriptor rings.
	// tail == head     => All descriptors belong to SW.
	// tail == head - 1 => All descriptors belong to HW.
	// On reset, Tail and Head are both zero.
	// So we only need to set one value, tail of rx ring.
	writel(i82540EM_SETTING_RX_BUFFER_COUNT - 1, i82540EM_dev->regs + i82540EM_RDT);

	// Initialize the transmitter.
	// 0x40 << 12 COLD setting as per doc.
	writel(i82540EM_TCTL_BITMASK_EN | 0x40 << 12, i82540EM_dev->regs + i82540EM_TCTL);

	// Initialize the receiver.
	// TODO: Don't accept all packets? Set normal values and test that they work.
	writel(	i82540EM_RCTL_BITMASK_EN  |
		i82540EM_RCTL_BITMASK_BAM |
		i82540EM_RCTL_BITMASK_UPE |
		i82540EM_RCTL_BITMASK_MPE,
		i82540EM_dev->regs + i82540EM_RCTL);

	// Initialize the RX Tasklet. Must be done before interrupts are enabled.
	tasklet_init(&i82540EM_dev->rx_tasklet, rx_data, (unsigned long int)i82540EM_dev);

	// Set the desired interrupts.
	// We want RXTO, RXO, and RXDMT0.
	writel(i82540EM_INTERRUPT_BITMASK_RXT0 	 |
	       i82540EM_INTERRUPT_BITMASK_RXO 	 |
               i82540EM_INTERRUPT_BITMASK_RXDMT0,
	       i82540EM_dev->regs + i82540EM_IMS);

	// Clear pending interrupts.
	readl(i82540EM_dev->regs + i82540EM_ICR);

	// Request IRQ, and mark as accquired.
	error = request_irq(pci_dev->irq, i82540EM_isr, IRQF_SHARED, "i82540EM", i82540EM_dev);
	if(error){
		dev_err(&pci_dev->dev, "Failed requesting IRQ, exiting.\n");
		goto err_request_irq;
	}
	i82540EM_dev->irq_accquired = 1;

	// REGISTER THE DEVICE.
	error = register_netdev(net_dev);
	if(error){
		dev_err(&pci_dev->dev, "i82540EM: Failed registering net device. Exiting.\n");
		goto err_netdev_register;
	}

	// Done!
	uart_print("i82540EM: Init completed successfully.\n");

	return 0;

err_netdev_register:
	if(i82540EM_dev->irq_accquired){
		free_irq(pci_dev->irq, i82540EM_dev);
		i82540EM_dev->irq_accquired = 0;
	}

err_request_irq:
	i82540EM_unmap_dma_mappings(i82540EM_dev);

err_init_dma_mappings:
	if(i82540EM_dev->regs){
		iounmap(i82540EM_dev->regs);
		i82540EM_dev->regs = 0;
	}

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

	struct net_device *net_dev = pci_get_drvdata(pci_dev);

	uart_print("i82540EM: i82540EM_remove(): Removing device.\n");

	// If the pci device has an associated net device object, remove it.
	if (net_dev){

		struct i82540EM *i82540EM_dev = netdev_priv(net_dev);

		// Unregister first to stop all activity.
		unregister_netdev(net_dev);

		if(i82540EM_dev->irq_accquired)
			free_irq(pci_dev->irq, i82540EM_dev);

		if(i82540EM_dev->regs){
			iounmap(i82540EM_dev->regs);
			i82540EM_dev->regs = 0;
		}

		i82540EM_unmap_dma_mappings(i82540EM_dev);

		// This erases our i82540EM private driver structure.
		free_netdev(net_dev);

		pci_release_regions(pci_dev);

	}

	if(pci_is_enabled(pci_dev))
		pci_disable_device(pci_dev);


}

// Transmit here.
static netdev_tx_t tx_data(struct sk_buff *tx_skb_buffer, struct net_device *dev){

	struct i82540EM *i82540EM_dev = netdev_priv(dev);
	u32 tail = 0;

	uart_print("tx_data(): Transmitting...\n");

	// Dump complete packet. Debug.
	uart_print("tx_data(): Dumping TX data.\n:");
	uart_print("tx_data(): tx_skb_buffer address: %llx\n", 			i82540EM_dev->tx_skb_buffer);
	uart_print("tx_data(): tx_skb_buffer->len: %d\n", 			i82540EM_dev->tx_skb_buffer->len);
	uart_print("tx_data(): tx_skb_buffer->head address: %llx\n", 		i82540EM_dev->tx_skb_buffer->head);
	uart_print("tx_data(): tx_skb_buffer->head physical address: %llx\n", 	virt_to_phys(i82540EM_dev->tx_skb_buffer->head));
	uart_print("tx_data(): TX packet bytes:\n");
	buffer_uart_print(i82540EM_dev->tx_skb_buffer->head, i82540EM_dev->tx_skb_buffer->len, 16);
	uart_print("\n");

	// Debugging.
	if(tx_skb_buffer->len > i82540EM_SETTING_ETHERNET_MTU || tx_skb_buffer->len > i82540EM_SETTING_TX_BUFFER_SIZE){
		uart_print("tx_data(): Oversized packet. Dropping.\n");
		kfree_skb(tx_skb_buffer); // NOT SURE!
		return NETDEV_TX_BUSY;
	}

	//u32 head = readl(i82540EM_dev->regs + i82540EM_TDH);
	tail = readl(i82540EM_dev->regs + i82540EM_TDT);

	// Check if we have a free descriptor at tail to write to.
	if(i82540EM_dev->tx_descriptors[tail].status != i82540EM_TX_STATUS_BITMASK_DD){
		uart_print("tx_data(): No free descriptor. Dropping.\n");
		kfree_skb(tx_skb_buffer); // NOT SURE!
		return NETDEV_TX_BUSY;
	}

	// Otherwise, descriptor is free. Use it.

	// Clear the status bit.
	i82540EM_dev->tx_descriptors[tail].status  = 0;

	// Set the command bit to report status and end of packet.
	// We're assuming single-descriptor packets sincke descriptor size > MTU of 1500.
	i82540EM_dev->tx_descriptors[tail].command = i82540EM_TX_COMMAND_BITMASK_EOP | i82540EM_TX_COMMAND_BITMASK_RS;

	// Copy the skb buffer data.
	memcpy(i82540EM_dev->tx_buffers + tail * i82540EM_SETTING_TX_BUFFER_SIZE, tx_skb_buffer->data, tx_skb_buffer->len);

	// Increment the tail pointer, this starts the transmission.
	writel((tail + 1) % i82540EM_SETTING_TX_BUFFER_COUNT, i82540EM_dev->regs + i82540EM_TDT);

	// Free the sk buffer.
	kfree_skb(tx_skb_buffer); // Probably.

	uart_print("tx_data(): Transmitted packet!\n");

	// Return success.
	return NETDEV_TX_OK;

}

static const struct net_device_ops i82540EM_net_ops = {
	//.ndo_open	= i82540EM_open,
	//.ndo_stop	= i82540EM_close,
	.ndo_start_xmit	= tx_data
};

static struct pci_driver i82540EM_pci_driver = {
	.name 		= DRIVER_NAME,
	.id_table 	= i82540EM_pci_tbl,
	.probe		= i82540EM_probe,
	.remove		= i82540EM_remove,
};

module_pci_driver(i82540EM_pci_driver)
