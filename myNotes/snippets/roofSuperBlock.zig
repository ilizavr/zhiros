//TODO:
//*use RB-threes for findind a free continuous sectors for file in findContinuousBlock
//*write  writeFIleData function that firstly tries find a continouous block by findContinuousBlock and if it gone fail,write file sector-by-sector  by use ataWriteFreeSector
//*write writeInode function that makes new inode record by take file's data, and write new inode0 record  where the name is name of a file
//*please test it. test on real Linux system or as if you want in other way. but please test it
//*make RoofFS works on Linux system. implement Linux-VFS interface and try to use RoofFS on a Linux system
const std = @import("std");

const fileType = enum(u3) {
    file,
    directory,
    symLink,
};

const roofSuperBlock = struct {
    maigc: u32,
    sectorSize: u64,
    diskSize: u64,
    inode0StartSector: u16,
    inode0EndSector: u32,
    ownerName: [20]u8,
    uuid: [16]u8,
    flags: u8,
    bitmapFreeSectors: std.ArrayList(u64),
    padding: u8,
};

const inode0Peer = struct {
    name: [28]u32,
    startAndEndSector: u128,
};

const roofInode0 = struct {
    magic: u16,
    inodeArray: std.ArrayList(inode0Peer),
};

const roofInode = struct {
    isFragmented: bool,
    fileType: fileType,
    linkCount: u8,
    data: union {
        unfragmented: struct {
            start: u64,
            end: u64,
        },
        fragmented: struct {
            starts: std.ArrayList(u64),
            ends: std.ArrayList(u64),
        },
    },
};

pub fn inb(port: u16) u8 {
    return asm volatile ("inb %[port],%[ret]"
        : [ret] "={ax}" (-> u8),
        : [port] "{dx}" (port),
    );
}

pub fn outb(port: u16, imm: u8) void {
    return asm volatile ("outb %[port],%[val]"
        :
        : [port] "{dx}" (port),
          [val] "{ax}" (imm),
    );
}

pub fn inw(port: u16) u16 {
    return asm volatile ("inw %[port],%[ret]"
        : [ret] "={ax}" (-> u16),
        : [port] "{dx}" (port),
    );
}

pub fn outw(port: u16, imm: u16) void {
    return asm volatile ("outw %[port],%[val]"
        :
        : [port] "{dx}" (port),
          [val] "{ax}" (imm),
    );
}

pub fn ataWriteSector(sectorAddress: u32, buffer: [512]u8, isImportant: bool) void {
    while (inb(0x1F7) & 0x80 != 0) {}
    outb(0x1F6, 0xE0);
    outb(0x1F2, 1);
    outb(0x1F3, @as(u8, (sectorAddress & 0x000000FF) >> 0));
    outb(0x1F4, @as(u8, (sectorAddress & 0x0000FF00) >> 8));
    outb(0x1F5, @as(u8, (sectorAddress & 0x00FF0000) >> 16));
    outb(0x1F6, @as(u8, (sectorAddress & 0xFF000000) >> 24));
    while (inb(0x1F7) & 0x08 != 0) {}

    outb(0x1F7, 0x30);

    while (inb(0x1F7) & 0x08 != 0) {}

    const words = @as([256]u16, @ptrCast(buffer));
    for (words) |word| {
        outw(0x1F0, word);
    }

    if (isImportant) {
        outb(0x1F7, 0xE7);
    }
}

pub fn ataReadSector(sectorAddress: u32, buffer: *[512]u8) void {
    while (inb(0x1F7) & 0x80 != 0) {}
    outb(0x1F6, 0xE0);
    outb(0x1F2, 1);
    outb(0x1F3, @as(u8, sectorAddress & 0x000000FF));
    outb(0x1F4, @as(u8, (sectorAddress & 0x0000FF00) >> 8));
    outb(0x1F5, @as(u8, (sectorAddress & 0x00FF0000) >> 16));
    outb(0x1F6, @as(u8, (sectorAddress & 0xFF000000) >> 24));
    while (inb(0x1F7) & 0x08 != 0) {}

    outb(0x1F7, 0x20);

    while (inb(0x1F7) & 0x08 != 0) {}

    const words = @as(*[256]u16, @ptrCast(buffer));
    for (words) |*word| {
        word.* = inw(0x1F0);
    }
}

pub fn ataWriteFreeSector(sb: *roofSuperBlock, buffer: [512]u8, isImportant: bool, allocator: std.mem.Allocator) !u32 {
    var clusterCount: u16 = 0;
    var sectorCountAtCluster: u8 = 0;

    while (clusterCount <= sb.bitmapFreeSectors.items.len) : (clusterCount += 1) {
        if (clusterCount >= sb.bitmapFreeSectors.items.len) {
            try sb.bitmapFreeSectors.append(0);
        }

        var bitmapMember: u64 = ~(sb.bitmapFreeSectors.items[clusterCount]);

        if (bitmapMember == 0) {
            try sb.bitmapFreeSectors.append(0);
            continue;
        }

        var bitmapMemberArr: [64]u1 = @bitCast([64]u1, bitmapMember);

        while (sectorCountAtCluster < bitmapMemberArr.len) : (sectorCountAtCluster += 1) {
            if (bitmapMemberArr[sectorCountAtCluster] == 1) {
                break;
            }
        }

        var uselessSectorCount: u8 = sectorCountAtCluster;
        while (uselessSectorCount < bitmapMemberArr.len) : (uselessSectorCount += 1) {
            bitmapMemberArr[uselessSectorCount] = 0;
        }

        sb.bitmapFreeSectors.items[clusterCount] = @bitCast(u64, bitmapMemberArr);
        break;
    }

    var lbaAddr: u64 = (clusterCount * 64) + sectorCountAtCluster;
    ataWriteSector(@intCast(u32, lbaAddr), buffer, isImportant);
    return @intCast(u32, lbaAddr);
}

pub fn ataWriteSectors(sectorCount: u16, sectorAddress: u32, buffer: []u8, isImportant: bool) void {
    var i: u16 = 0;
    var changingSectorAddress: u32 = sectorAddress;
    var sectorsSize: u32 = 512;
    var sectorBuffer: [512]u8 = undefined;

    while (i < sectorCount) : (i += 1) {
        @memcpy(&sectorBuffer, buffer[i * sectorsSize .. (i + 1) * sectorsSize]);
        ataWriteSector(changingSectorAddress, sectorBuffer, isImportant);
        changingSectorAddress += 1;
    }
}

pub fn ataReadSectors(sectorCount: u16, sectorAddress: u32, buffer: []u8) void {
    var i: u16 = 0;
    var changingSectorAddress: u32 = sectorAddress;
    var sectorsSize: u32 = 512;
    var sectorBuffer: [512]u8 = undefined;

    while (i < sectorCount) : (i += 1) {
        ataReadSector(changingSectorAddress, &sectorBuffer);
        @memcpy(buffer[i * sectorsSize .. (i + 1) * sectorsSize], &sectorBuffer);
        changingSectorAddress += 1;
    }
}

pub fn changeOwnerName(sb: roofSuperBlock, ownerName: [20]u8) void {
    const buffer: [512]u8 = @as([512]u8, @bitCast([@sizeOf(sb)]u8, sb));
    ataWriteSector(2, buffer, true);
}

pub fn initRoofSB() roofSuperBlock {
    var sb: roofSuperBlock = {};

    while (inb(0x1F7) & 0x80 != 0) {}
    outb(0x1F6, 0xE0);
    outb(0x1F7, 0xA1);
    while (inb(0x1F7) & 0x08 != 0) {}

    var diskData: [256]u16 = [256]u16{};
    var i: u16 = 0;
    while (i < 256) : (i += 1) {
        diskData[i] = inw(0x1F0);
    }

    const word106 = diskData[106];
    var sectorSize: u16 = 0;
    if (word106 & (1 << 13)) {
        sectorSize = diskData[117] * 2;
        sb.sectorSize = sectorSize;
    }

    if (!diskData[83] & (1 << 10)) {
        const sectorCount = diskData[60] | (diskData[61] << 16);
        var diskSize: u64 = sectorCount * sectorSize;
        sb.diskSize = diskSize;
    }
    std.crypto.random.bytes(&sb.uuid);

    sb.uuid[6] = (sb.uuid & 0x0F) | 0x40;
    sb.uuid[8] = (sb.uuid & 0x3F) | 0x80;

    sb.flags = 0xC0;
    sb.magic = 0x524F4FF;
    sb.inode0StartSector = 3;
}

pub fn writeInode0Record(rI0: roofInode0, sb: roofSuperBlock, name: [24]u32, startEndSector: u128) roofInode0 {
    var inode0ArrBuf: std.ArrayList(@TypeOf(rI0.inodeArray)) = std.ArrayList(@TypeOf(rI0.inodeArray));

    for (rI0.inodeArray) |rec| {
        try inode0ArrBuf.append(rec);
    }

    var newRec: []inode0Peer = []inode0{
        .name = name,
        .startAndEndSector = startEndSector,
    };

    try inode0ArrBuf.append(newRec);

    var newRI0: roofInode0 = .{
        .maigc = 0x4332,
        .inodeAray = undefined,
    };
    for (inode0ArrBuf.items, 0..) |item, i| {
        newRI0[i] = item;
    }

    sb.inode0EndSector += 1;
    var foo: [512]u8 = @bitCast([512]u8, sb);
    ataWriteSector(2, foo, true);
    return newRI0;
}

pub fn findContinuousBlock(sb: roofSuperBlock,needed: u32) ?u64{

}

