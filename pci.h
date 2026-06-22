u16 pci_read_word(u8 bus, u8 slot, u8 func, u8 offset)
{
	u32 address = (u32)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
	port_dword_out(0xCF8, address);
	return (u16)((port_dword_in(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
}

u32 pci_read_dword(u8 bus, u8 slot, u8 func, u8 offset)
{
	u32 address = (u32)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
	port_dword_out(0xCF8, address);
	return port_dword_in(0xCFC);
}
volatile u32 addressE1000;

void check_e1000(u32 bus, u32 slot, u32 func){
  u16 vendor_id = pci_read_word(bus, slot, func, 0x0);
  u16 device_id = pci_read_word(bus, slot, func, 0x02);
  if(vendor_id == 0x8086 && device_id == 0x100E){ 
addressE1000 = pci_read_dword(bus, slot, func, 0x10) & 0xFFFFFFF0;
  print("addressE1000: 0x"); 
print_hex(addressE1000); 
print("\n");
  }
}

void pci_check_disk(u8 bus, u8 slot, u8 func)
{
	u16 vendor_id = pci_read_word(bus, slot, func, 0x00);
	if (vendor_id == 0xFFFF) return; 
    
	u16 class_word = pci_read_word(bus, slot, func, 0x0A);
        u8 class = (class_word >> 8) & 0xFF;
	if(class!=1) return;
	
	u32 bar0 = pci_read_dword(bus, slot, func, 0x10);

	u32 bar1 = pci_read_dword(bus, slot, func, 0x14);

	u16 base, control;

	if (bar0 & 0x1) {
		base = (u16)(bar0 & 0xFFFC);
		control = (u16)(bar1 & 0xFFFC)+2;
    	}else{
		KLOGW("MMIO not supported. try to use legacy ports\n");
		base=0x1F0;
		control=0x3F6;
	}

	ide_add_disk(base,control);

}

void find_pci_devices()
{	
    for (u32 bus = 0; bus < 256; bus++)
    {
        for (u8 slot = 0; slot < 32; slot++)
        {
            u16 vendor_id = pci_read_word(bus, slot, 0, 0x00);
            if (vendor_id == 0xFFFF) continue; 

            pci_check_disk(bus, slot, 0);
            check_e1000(bus, slot, 0);
	    

            u16 header_type = pci_read_word(bus, slot, 0, 0x0E);
            if (header_type & 0x80) 
            {
                for (u8 func = 1; func < 8; func++)
                {
                    pci_check_disk(bus, slot, func);
            	    check_e1000(bus, slot, func);
                }
            }
        }
    }
}

void pci_check_device(u8 bus, u8 slot, u8 func)
{
	u16 vendor_id = pci_read_word(bus, slot, func, 0x00);
	if (vendor_id == 0xFFFF) return; 

	u16 device_id = pci_read_word(bus, slot, func, 0x02);
    
	u16 class_word = pci_read_word(bus, slot, func, 0x0A);
	u8 subclass = class_word & 0xFF;
	u8 class_code = (class_word >> 8) & 0xFF;
	
	print("PCI [B:");   print_int(bus);
	print(" D:");       print_int(slot);
	print(" F:");       print_int(func);
	print("] Vendor:0x"); print_hex(vendor_id);
	print(" Device:0x"); print_hex(device_id);
	print(" Class:0x");  print_hex(class_code);
	print(" Sub:0x");    print_hex(subclass);
	print("\n");
}

void pci_scan(void)
{
    for (u32 bus = 0; bus < 256; bus++)
    {
        for (u8 slot = 0; slot < 32; slot++)
        {
            u16 vendor_id = pci_read_word(bus, slot, 0, 0x00);
            if (vendor_id == 0xFFFF) continue; 

            pci_check_device(bus, slot, 0);

            u16 header_type = pci_read_word(bus, slot, 0, 0x0E);
            if (header_type & 0x80) 
            {
                for (u8 func = 1; func < 8; func++)
                {
                    pci_check_device(bus, slot, func);
                }
            }
        }
    }
}


