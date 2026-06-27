struct __attribute__((packed)) RSDP {
    char Signature[8];
    u8 Checksum;
    char OEMID[6];
    u8 Revision;
    u32 RsdtAddress;
};
struct __attribute__((packed)) ACPISDTHeader {
    char Signature[4];
    u32 Length;
    u8 Revision;
    u8 Checksum;
    char OEMID[6];
    char OEMTableID[8];
    u32 OEMRevision;
    u32 CreatorID;
    u32 CreatorRevision;
};

struct __attribute__((packed)) FADT
{
    struct ACPISDTHeader h;
    u32 FirmwareCtrl;
    u32 Dsdt;
    u8  Reserved;
    u8  PreferredPowerManagementProfile;
    u16 SCI_errupt;
    u32 SMI_CommandPort;
    u8  AcpiEnable;
    u8  AcpiDisable;
    u8  S4BIOS_REQ;
    u8  PSTATE_Control;
    u32 PM1aEventBlock;
    u32 PM1bEventBlock;
    u32 PM1aControlBlock;
    u32 PM1bControlBlock;
    u32 PM2ControlBlock;
    u32 PMTimerBlock;
    u32 GPE0Block;
    u32 GPE1Block;
    u8  PM1EventLength;
    u8  PM1ControlLength;
    u8  PM2ControlLength;
    u8  PMTimerLength;
    u8  GPE0Length;
    u8  GPE1Length;
    u8  GPE1Base;
    u8  CStateControl;
    u16 WorstC2Latency;
    u16 WorstC3Latency;
    u16 FlushSize;
    u16 FlushStride;
    u8  DutyOffset;
    u8  DutyWidth;
    u8  DayAlarm;
    u8  MonthAlarm;
    u8  Century;

    //others field 
};

int checksum(u8*a,int len)
{
    u8 sum = 0;
    for(int i = 0;i<len;i++){
        sum+=a[i];
    }
    return sum;
}

struct RSDP* find_rsdp()
{
    for(char* a = (char*)0xE0000;a<(char*)0xFFFFF;a+=0x10){
        if(memcmp(a,"RSD PTR ",8) == 0){
            if(checksum(a,20)) continue;
            return (struct RSDP*)a;
        }
    }
    return 0;
}

struct FADT* fadt = 0;

void find_fadt()
{
    struct RSDP* rsdp = find_rsdp();
    if(!rsdp){
        KLOGE("RSDP not found\n");
        return;
    }
    char *rsdt_sign = (char*)rsdp->RsdtAddress;
    if(memcmp(rsdt_sign,"RSDT",4)!=0) {
        KLOGE("RSDT not valid\n");
        return;
    }

    u8** rsdt = (u8**)(rsdp->RsdtAddress+36);
    while(1){
        if(memcmp(*rsdt,"FACP",4) == 0) {
            fadt = (struct FADT*)*rsdt;
            return;
        }
        rsdt++;
    }
}

void enableacpi()
{
    find_fadt();
    if(fadt){
        outb(fadt->SMI_CommandPort,fadt->AcpiEnable);
        KLOGI("acpi enabled. portb ");
        print_int(fadt->PM1bControlBlock);
        print("\n");
    }
}
