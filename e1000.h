
void e1000_read(u32 BUSMASTER, u32 MEMORYSPACE){             //permission read base address true
	u32 pci_config_address = (u32)((busG << 16) | (slotG << 11) | (0 << 8) | (0x04 & 0xfc) | ((u32)0x80000000));
	u32 addressRequest = pci_config_address;
	u32 value;
	outl(0xCF8, addressRequest);
	print("\n"); print("addressRequest "); print_hex(addressRequest); print("\n");
	value = inl(0xCFC);
	print("value not changed: ");print_hex(value); print("\n");
	
	value |= (BUSMASTER << 2);
	value |= (MEMORYSPACE << 1);
	print("\n"); print("changed: "); print_hex(value); print("\n");
	outl(0xCFC, value);
}
