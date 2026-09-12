#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID 0x100E

#define E1000_REG_CTRL 0x0000
#define E1000_REG_STATUS 0x0008
#define E1000_REG_ICR 0x00C0
#define E1000_REG_IMC 0x00D8
#define E1000_REG_RCTL 0x0100
#define E1000_REG_TCTL 0x0400
#define E1000_REG_RDBAL 0x2800
#define E1000_REG_RDBAH 0x2804
#define E1000_REG_RDLEN 0x2808
#define E1000_REG_RDH 0x2810
#define E1000_REG_RDT 0x2818
#define E1000_REG_TDBAL 0x3800
#define E1000_REG_TDBAH 0x3804
#define E1000_REG_TDLEN 0x3808
#define E1000_REG_TDH 0x3810
#define E1000_REG_TDT 0x3818
#define E1000_REG_RAL 0x5400
#define E1000_REG_RAH 0x5404

#define E1000_CTRL_RESET (1u << 26)
#define E1000_STATUS_LINK_UP (1u << 1)
#define E1000_RCTL_EN (1u << 1)
#define E1000_RCTL_BAM (1u << 15)
#define E1000_RCTL_SECRC (1u << 26)
#define E1000_TCTL_EN (1u << 1)
#define E1000_TCTL_PSP (1u << 3)
#define E1000_TCTL_CT_SHIFT 4
#define E1000_TCTL_COLD_SHIFT 12
#define E1000_RAH_ADDRESS_VALID (1u << 31)
#define E1000_TX_STATUS_DONE 1
#define E1000_RESET_WAIT_MAX 100000
#define E1000_DMA_ALIGNMENT 4096
#define E1000_RX_DESC_COUNT 16
#define E1000_TX_DESC_COUNT 16
#define E1000_PACKET_BUFFER_SIZE 2048
#define E1000_FRAME_MAX 1518
#define E1000_RX_STATUS_DONE 1
#define E1000_RX_STATUS_EOP 2
#define E1000_TX_CMD_EOP 1
#define E1000_TX_CMD_IFCS 2
#define E1000_TX_CMD_RS 8
#define E1000_TX_WAIT_MAX 1000000

__attribute__((packed)) struct e1000_rx_descriptor
{
	u64 address;
	u16 length;
	u16 checksum;
	u8 status;
	u8 errors;
	u16 special;
};

__attribute__((packed)) struct e1000_tx_descriptor
{
	u64 address;
	u16 length;
	u8 checksum_offset;
	u8 command;
	u8 status;
	u8 checksum_start;
	u16 special;
};

struct e1000_dma_block
{
	void *raw;
	u8 *aligned;
	u32 size;
};

struct e1000_dma_arena
{
	struct e1000_dma_block rx_ring;
	struct e1000_dma_block tx_ring;
	struct e1000_dma_block rx_buffers;
	struct e1000_dma_block tx_buffers;
	u32 ready;
};

struct e1000_state
{
	u8 bus;
	u8 slot;
	u8 function;
	u32 mmio;
	u8 mac[6];
	u32 link_up;
	u32 initialized;
	struct e1000_dma_arena dma;
	u32 rx_index;
	u32 tx_index;
	u32 rx_packets;
	u32 rx_drops;
	u32 tx_packets;
	u32 tx_drops;
};

static struct e1000_state e1000_device;

static u32 e1000_mmio_read(u32 offset)
{
	if (!e1000_device.mmio) return 0;
	return *(volatile u32 *)(e1000_device.mmio + offset);
}

static void e1000_mmio_write(u32 offset, u32 value)
{
	if (!e1000_device.mmio) return;
	*(volatile u32 *)(e1000_device.mmio + offset) = value;
}

static int e1000_dma_block_alloc(u32 size, struct e1000_dma_block *block)
{
	if (!size || !block || size > 0xFFFFFFFFu - (E1000_DMA_ALIGNMENT - 1))
		return -1;
	void *raw = kalloc(size + E1000_DMA_ALIGNMENT - 1);
	if (!raw) return -1;
	u32 raw_address = (u32)raw;
	if (raw_address > 0xFFFFFFFFu - (E1000_DMA_ALIGNMENT - 1)) {
		free(raw);
		return -1;
	}
	u32 aligned_address =
		(raw_address + E1000_DMA_ALIGNMENT - 1) &
		~(E1000_DMA_ALIGNMENT - 1);
	block->raw = raw;
	block->aligned = (u8 *)aligned_address;
	block->size = size;
	return 0;
}

static void e1000_dma_block_release(struct e1000_dma_block *block)
{
	if (!block) return;
	if (block->raw) free(block->raw);
	block->raw = 0;
	block->aligned = 0;
	block->size = 0;
}

static void e1000_dma_cleanup()
{
	e1000_dma_block_release(&e1000_device.dma.tx_buffers);
	e1000_dma_block_release(&e1000_device.dma.rx_buffers);
	e1000_dma_block_release(&e1000_device.dma.tx_ring);
	e1000_dma_block_release(&e1000_device.dma.rx_ring);
	e1000_device.dma.ready = 0;
}

static int e1000_dma_prepare()
{
	struct e1000_dma_arena *dma = &e1000_device.dma;
	if (dma->ready) return -1;
	memset(dma, 0, sizeof(*dma));
	if (e1000_dma_block_alloc(
			E1000_RX_DESC_COUNT * sizeof(struct e1000_rx_descriptor),
			&dma->rx_ring))
		return -1;
	if (e1000_dma_block_alloc(
			E1000_TX_DESC_COUNT * sizeof(struct e1000_tx_descriptor),
			&dma->tx_ring))
		goto fail;
	if (e1000_dma_block_alloc(
			E1000_RX_DESC_COUNT * E1000_PACKET_BUFFER_SIZE,
			&dma->rx_buffers))
		goto fail;
	if (e1000_dma_block_alloc(
			E1000_TX_DESC_COUNT * E1000_PACKET_BUFFER_SIZE,
			&dma->tx_buffers))
		goto fail;
	memset(dma->rx_ring.aligned, 0, dma->rx_ring.size);
	memset(dma->tx_ring.aligned, 0, dma->tx_ring.size);
	memset(dma->rx_buffers.aligned, 0, dma->rx_buffers.size);
	memset(dma->tx_buffers.aligned, 0, dma->tx_buffers.size);
	struct e1000_rx_descriptor *rx =
		(struct e1000_rx_descriptor *)dma->rx_ring.aligned;
	struct e1000_tx_descriptor *tx =
		(struct e1000_tx_descriptor *)dma->tx_ring.aligned;
	for (u32 index = 0; index < E1000_RX_DESC_COUNT; index++) {
		rx[index].address = (u64)(u32)(dma->rx_buffers.aligned +
			index * E1000_PACKET_BUFFER_SIZE);
		rx[index].status = 0;
	}
	for (u32 index = 0; index < E1000_TX_DESC_COUNT; index++) {
		tx[index].address = (u64)(u32)(dma->tx_buffers.aligned +
			index * E1000_PACKET_BUFFER_SIZE);
		tx[index].status = E1000_TX_STATUS_DONE;
	}
	dma->ready = 1;
	return 0;
fail:
	e1000_dma_cleanup();
	return -1;
}

static int e1000_reset()
{
	e1000_mmio_write(E1000_REG_CTRL,
	                 e1000_mmio_read(E1000_REG_CTRL) | E1000_CTRL_RESET);
	for (u32 wait = 0; wait < E1000_RESET_WAIT_MAX; wait++)
		if (!(e1000_mmio_read(E1000_REG_CTRL) & E1000_CTRL_RESET)) break;
	if (e1000_mmio_read(E1000_REG_CTRL) & E1000_CTRL_RESET) return -1;
	e1000_mmio_write(E1000_REG_IMC, 0xFFFFFFFF);
	(void)e1000_mmio_read(E1000_REG_ICR);
	return 0;
}

static int e1000_read_mac()
{
	u32 low = e1000_mmio_read(E1000_REG_RAL);
	u32 high = e1000_mmio_read(E1000_REG_RAH);
	if ((high & E1000_RAH_ADDRESS_VALID) == 0) return -1;
	e1000_device.mac[0] = (u8)low;
	e1000_device.mac[1] = (u8)(low >> 8);
	e1000_device.mac[2] = (u8)(low >> 16);
	e1000_device.mac[3] = (u8)(low >> 24);
	e1000_device.mac[4] = (u8)high;
	e1000_device.mac[5] = (u8)(high >> 8);
	return 0;
}

static int e1000_configure_rings()
{
	if (!e1000_device.dma.ready) return -1;
	u32 rx_ring = (u32)e1000_device.dma.rx_ring.aligned;
	u32 tx_ring = (u32)e1000_device.dma.tx_ring.aligned;
	e1000_mmio_write(E1000_REG_RDBAL, rx_ring);
	e1000_mmio_write(E1000_REG_RDBAH, 0);
	e1000_mmio_write(E1000_REG_RDLEN,
	                 E1000_RX_DESC_COUNT * sizeof(struct e1000_rx_descriptor));
	e1000_mmio_write(E1000_REG_RDH, 0);
	e1000_mmio_write(E1000_REG_RDT, E1000_RX_DESC_COUNT - 1);
	e1000_mmio_write(E1000_REG_TDBAL, tx_ring);
	e1000_mmio_write(E1000_REG_TDBAH, 0);
	e1000_mmio_write(E1000_REG_TDLEN,
	                 E1000_TX_DESC_COUNT * sizeof(struct e1000_tx_descriptor));
	e1000_mmio_write(E1000_REG_TDH, 0);
	e1000_mmio_write(E1000_REG_TDT, 0);
	e1000_mmio_write(E1000_REG_TCTL,
	                 E1000_TCTL_EN | E1000_TCTL_PSP |
	                 (0x10u << E1000_TCTL_CT_SHIFT) |
	                 (0x40u << E1000_TCTL_COLD_SHIFT));
	e1000_mmio_write(E1000_REG_RCTL,
	                 E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SECRC);
	e1000_device.rx_index = 0;
	e1000_device.tx_index = 0;
	return 0;
}

static void e1000_memory_barrier()
{
	asm volatile ("" ::: "memory");
}

int e1000_send_frame(const void *frame, u32 length)
{
	if (!e1000_device.initialized || !e1000_device.dma.ready || !frame ||
	    !length || length > E1000_FRAME_MAX) return -1;
	volatile struct e1000_tx_descriptor *ring =
		(volatile struct e1000_tx_descriptor *)e1000_device.dma.tx_ring.aligned;
	u32 index = e1000_device.tx_index;
	volatile struct e1000_tx_descriptor *desc = &ring[index];
	if (!(desc->status & E1000_TX_STATUS_DONE)) return -2;
	u8 *buffer = e1000_device.dma.tx_buffers.aligned +
		index * E1000_PACKET_BUFFER_SIZE;
	memcpy(buffer, frame, length);
	desc->length = (u16)length;
	desc->checksum_offset = 0;
	desc->command = E1000_TX_CMD_EOP | E1000_TX_CMD_IFCS | E1000_TX_CMD_RS;
	desc->status = 0;
	desc->checksum_start = 0;
	desc->special = 0;
	e1000_memory_barrier();
	e1000_mmio_write(E1000_REG_TDT, (index + 1) % E1000_TX_DESC_COUNT);
	for (u32 wait = 0; wait < E1000_TX_WAIT_MAX; wait++)
		if (desc->status & E1000_TX_STATUS_DONE) break;
	if (!(desc->status & E1000_TX_STATUS_DONE)) {
		e1000_device.tx_drops++;
		return -3;
	}
	e1000_device.tx_index = (index + 1) % E1000_TX_DESC_COUNT;
	e1000_device.tx_packets++;
	return 0;
}

int e1000_receive_frame(void *frame, u32 capacity, u32 *length)
{
	if (length) *length = 0;
	if (!e1000_device.initialized || !e1000_device.dma.ready || !frame ||
	    !capacity || !length) return -1;
	volatile struct e1000_rx_descriptor *ring =
		(volatile struct e1000_rx_descriptor *)e1000_device.dma.rx_ring.aligned;
	u32 index = e1000_device.rx_index;
	volatile struct e1000_rx_descriptor *desc = &ring[index];
	if (!(desc->status & E1000_RX_STATUS_DONE)) return 1;
	u16 packet_length = desc->length;
	int result = 0;
	if (!(desc->status & E1000_RX_STATUS_EOP) || desc->errors ||
	    !packet_length || packet_length > E1000_PACKET_BUFFER_SIZE ||
	    packet_length > capacity) {
		result = -2;
	} else {
		u8 *buffer = e1000_device.dma.rx_buffers.aligned +
			index * E1000_PACKET_BUFFER_SIZE;
		memcpy(frame, buffer, packet_length);
		*length = packet_length;
	}
	desc->status = 0;
	desc->errors = 0;
	desc->length = 0;
	e1000_memory_barrier();
	e1000_mmio_write(E1000_REG_RDT, index);
	e1000_device.rx_index = (index + 1) % E1000_RX_DESC_COUNT;
	if (result) e1000_device.rx_drops++;
	else e1000_device.rx_packets++;
	return result;
}

int e1000_send_test_frame()
{
	u8 frame[60];
	memset(frame, 0, sizeof(frame));
	for (u32 index = 0; index < 6; index++) {
		frame[index] = 0xFF;
		frame[index + 6] = e1000_device.mac[index];
	}
	frame[12] = 0x88;
	frame[13] = 0xB5;
	for (u32 index = 14; index < sizeof(frame); index++)
		frame[index] = (u8)index;
	return e1000_send_frame(frame, sizeof(frame));
}

int e1000_probe()
{
	u8 bus;
	u8 slot;
	u8 function;
	if (e1000_device.initialized) return 0;
	if (pci_find_device(E1000_VENDOR_ID, E1000_DEVICE_ID,
	                    &bus, &slot, &function))
		return -1;
	u32 bar0 = pci_read_dword(bus, slot, function, 0x10);
	if (!bar0 || (bar0 & 1) || (bar0 & 6) == 4) return -2;
	u16 command = pci_read_word(bus, slot, function, 0x04);
	pci_write_word(bus, slot, function, 0x04, command | 0x0006);
	command = pci_read_word(bus, slot, function, 0x04);
	if ((command & 0x0006) != 0x0006) return -3;
	e1000_device.bus = bus;
	e1000_device.slot = slot;
	e1000_device.function = function;
	e1000_device.mmio = bar0 & 0xFFFFFFF0;
	if (e1000_reset()) return -4;
	if (e1000_read_mac()) return -5;
	if (e1000_dma_prepare()) return -6;
	if (e1000_configure_rings()) {
		e1000_dma_cleanup();
		return -7;
	}
	e1000_device.link_up = (e1000_mmio_read(E1000_REG_STATUS) &
	                        E1000_STATUS_LINK_UP) != 0;
	e1000_device.initialized = 1;
	return 0;
}

int e1000_get_mac(u8 *mac)
{
	if (!e1000_device.initialized || !mac) return -1;
	memcpy(mac, e1000_device.mac, sizeof(e1000_device.mac));
	return 0;
}

void e1000_print_info()
{
	if (!e1000_device.initialized) {
		KLOGW("E1000 is not initialized\n");
		return;
	}
	print("E1000 at B:");
	print_int(e1000_device.bus);
	print(" D:");
	print_int(e1000_device.slot);
	print(" F:");
	print_int(e1000_device.function);
	print(" BAR0: 0x");
	print_hex(e1000_device.mmio);
	print(" MAC:");
	for (u32 index = 0; index < 6; index++) {
		print(index ? ":" : " ");
		print_hex(e1000_device.mac[index]);
	}
	print(" LINK:");
	print(e1000_device.link_up ? "up" : "down");
	print(" DMA:");
	print(e1000_device.dma.ready ? "ready\n" : "not ready\n");
	print(" packets RX:");
	print_int(e1000_device.rx_packets);
	print(" TX:");
	print_int(e1000_device.tx_packets);
	print(" drops RX:");
	print_int(e1000_device.rx_drops);
	print(" TX:");
	print_int(e1000_device.tx_drops);
	print("\n");
	print(" RX ring: 0x");
	print_hex((u32)e1000_device.dma.rx_ring.aligned);
	print(" TX ring: 0x");
	print_hex((u32)e1000_device.dma.tx_ring.aligned);
	print(" RCTL: 0x");
	print_hex(e1000_mmio_read(E1000_REG_RCTL));
	print(" TCTL: 0x");
	print_hex(e1000_mmio_read(E1000_REG_TCTL));
	print("\n");
}
