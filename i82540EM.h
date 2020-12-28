#ifndef i82540EM_H
#define i82540EM_H

#define i82540EM_VENDOR 0x8086
#define i82540EM_DEVICE 0x100e

#define DRIVER_NAME "i82540EM Driver"

#define BAR_0 0

#define i82540EM_CTRL	 			0x0	   // Device Control Register Base.
#define i82540EM_CTRL_BITMASK_FD 		0x1        // Full-Duplex bitmask
#define i82540EM_CTRL_BITMASK_LRST 		0x8        // Link reset. Not applicable in 82540EM.
#define i82540EM_CTRL_BITMASK_ASDE 		0x20       // Auto speed detection bitmask.
#define i82540EM_CTRL_BITMASK_SLU 		0x40	   // Set Link Up
#define i82540EM_CTRL_BITMASK_ILOS 		0x80       // Invert Loss-Of-Signal
#define i82540EM_CTRL_BITMASK_SPEED 		0x300  	   // Speed
#define i82540EM_CTRL_BITMASK_FRCSPD 		0x800 	   // Force set speed.
#define i82540EM_CTRL_BITMASK_FRCDPLX 		0x1000     // Force duplex
#define i82540EM_CTRL_BITMASK_SDP0_DATA 	0x40000    // SDP0
#define i82540EM_CTRL_BITMASK_SDP1_DATA 	0x80000    // SDP1
#define i82540EM_CTRL_BITMASK_ADVD3WUC  	0x100000   // D3Cold Wakeup Capability Advertisement Enable.
#define i82540EM_CTRL_BITMASK_EN_PHY_PWR_MGMT 	0x200000   // Phy power-management enable.
#define i82540EM_CTRL_BITMASK_SDP0_IODIR	0x400000   // SDP0 Pin Directionality.
#define i82540EM_CTRL_BITMASK_SDP1_IODIR	0x800000   // SDP1 Pin Directionality.
#define i82540EM_CTRL_BITMASK_RST		0x4000000  // Device Reset.
#define i82540EM_CTRL_BITMASK_RFCE		0x8000000  // Receive Flow Control Enable.
#define i82540EM_CTRL_BITMASK_TFCE		0x10000000 // Transmit Flow Control Enable.
#define i82540EM_CTRL_BITMASK_VME		0x40000000 // Vlan Mode Enable
#define i82540EM_CTRL_BITMASK_PHY_RST		0x80000000 // PHY Reset

#define i82540EM_RCTL 			0x100 		// Receive Control Register Base.
#define i82540EM_RCTL_BITMASK_EN 	0x2		// Receiver Enable.
#define i82540EM_RCTL_BITMASK_SBP 	0x4		// Store Bad Packets.
#define i82540EM_RCTL_BITMASK_UPE 	0x8		// Unicast Promiscuous Enable.
#define i82540EM_RCTL_BITMASK_MPE 	0x10		// Multicast Promiscuous Enable.
#define i82540EM_RCTL_BITMASK_LPE 	0x20		// Long Packet Reception Enable.
#define i82540EM_RCTL_BITMASK_LBM 	0xC0		// Loopback Mode.
#define i82540EM_RCTL_BITMASK_RDMTS 	0x300		// Receive Descriptor Minimum Threshold.
#define i82540EM_RCTL_BITMASK_MO 	0x3000		// Multicast Offset.
#define i82540EM_RCTL_BITMASK_BAM 	0x8000		// Broadcast Accept Mode.
#define i82540EM_RCTL_BITMASK_BSIZE 	0x30000		// Receive Buffer Size.
#define i82540EM_RCTL_BITMASK_VFE 	0x40000 	// VLAN Filter Enable.
#define i82540EM_RCTL_BITMASK_CFIEN 	0x80000		// Canonical Form Indicator Enable.
#define i82540EM_RCTL_BITMASK_CFI 	0x100000	// Canonical Form Inidcator Bit Value
#define i82540EM_RCTL_BITMASK_DPF 	0x400000	// Discard Pause Frames
#define i82540EM_RCTL_BITMASK_PMCF 	0x800000	// Pass MAC Control Frames
#define i82540EM_RCTL_BITMASK_BSEX 	0x2000000	// Buffer Size Extension
#define i82540EM_RCTL_BITMASK_SECRC 	0x4000000	// Strip Ethernet CRC from inc packet.

#define i82540EM_FCAL  0x28  // Flow control address low.
#define i82540EM_FCAH  0x2C  // Flow control address high.
#define i82540EM_FCT   0x30  // Flow control type.
#define i82540EM_FCTTV 0x170 // Flow Control Transmit Timer.

#define i82540EM_ICR 0xC0 // Interrupt Cause Read register.
#define i82540EM_ICS 0xC8 // Interrupt Cause Set register.
#define i82540EM_IMS 0xD0 // Interrupt Mask Set/Read register.
#define i82540EM_IMC 0xD8 // Interrupt Mask Clear register.

#define i82540EM_INTERRUPT_BITMASK_TXDW 0x1        // Transmit Descriptor Written Back.
#define i82540EM_INTERRUPT_BITMASK_TXQE 0x2        // Transmit Queue Empty.
#define i82540EM_INTERRUPT_BITMASK_LSC 0x4         // Link Status Change.
#define i82540EM_INTERRUPT_BITMASK_RXSEQ 0x8       // Receive Sequence Error.
#define i82540EM_INTERRUPT_BITMASK_RXDMT0 0x10     // Receive Descriptor Minimum Threshold Reached.
#define i82540EM_INTERRUPT_BITMASK_RXO 0x40        // Receiver Overrun.
#define i82540EM_INTERRUPT_BITMASK_RXT0 0x80       // Receiver Timer Interrupt.
#define i82540EM_INTERRUPT_BITMASK_MDAC 0x200      // MDIO Access Complete.
#define i82540EM_INTERRUPT_BITMASK_RXCFG 0x400     // Receiving /C/ ordered sets.
#define i82540EM_INTERRUPT_BITMASK_PHYINT 0x1000   // PHY Interrupt.
#define i82540EM_INTERRUPT_BITMASK_GPI_SDP6 0x2000 // General Purpose Interrupt on SDP6.
#define i82540EM_INTERRUPT_BITMASK_GPI_SDP7 0x4000 // General Purpose Interrupt on SDP7.
#define i82540EM_INTERRUPT_BITMASK_TXD_LOW 0x8000  // Transmit Descriptor Low Threshold hit.
#define i82540EM_INTERRUPT_BITMASK_SRPD 0x10000    // Small Receive Packet Detected.

#define i82540EM_RAL 0x5400 // Receive Address Low
#define i82540EM_RAH 0x5404 // Receive Address High

#define i82540EM_MTA 0x5200 // MTA vector table
#define i82540EM_MTA_SIZE 128 // Entries in vector table

#define i82540EM_RDBAL 0x2800 // Receive Descriptor Base Address Low.
#define i82540EM_RDBAH 0x2804 // Receive Descriptor Base Address high.
#define i82540EM_RDLEN 0x2808 // Receive Descriptor Length in bytes.
#define i82540EM_RDH 0x2810 // Recieve Descriptor Head.
#define i82540EM_RDT 0x2818 // Receive Descritor Tail.

#define i82540EM_RX_DESCRIPTOR_SIZE sizeof(struct i82540EM_rx_descriptor) // Size of receive descriptor

#define i82540EM_SETTING_RX_BUFFER_COUNT 256 // Number of desired receive buffers
#define i82540EM_SETTING_RX_BUFFER_SIZE 2048 // Default (on-reset) size of buffers.

struct i82540EM_rx_descriptor{
	u32 addr_high;
	u32 addr_low;
	u16 special;
	u8  errors;
	u8  status;
	u16 checksum;
	u16 length;
};

struct i82540EM{

	// Spin lock protecting the oject.
	// Todo: Make this more granular in the future.
	spinlock_t lock;

	// Stores the network device parent object we're inside of.
	struct net_device *net_dev;

	// Store the pci device.
	struct pci_dev *pci_dev;

	// Memory-mapped registers.
	void *regs;

	// Pointer to receive descriptor ring
	struct i82540EM_rx_descriptor *rx_descriptors;

	// DMA handle to the rx descriptor ring
	dma_addr_t rx_descriptors_dma_handle;

	// Receive buffers.
	char *rx_buffers;
	dma_addr_t rx_buffers_dma_handle;

	// IRQ accquired
	char irq_accquired;

};

#endif // !(i82540EM_H)

