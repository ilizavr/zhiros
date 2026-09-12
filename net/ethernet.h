#define NET_ETHERNET_ADDRESS_LENGTH 6
#define NET_ETHERNET_HEADER_LENGTH 14
#define NET_ETHERNET_MIN_FRAME_LENGTH 60
#define NET_ETHERTYPE_IPV4 0x0800
#define NET_ETHERTYPE_ARP 0x0806
#define NET_PACKET_BUFFER_SIZE 2048

typedef int (*net_send_frame_function)(const void *, u32);
typedef int (*net_receive_frame_function)(void *, u32, u32 *);

__attribute__((packed)) struct net_ethernet_header
{
	u8 destination[NET_ETHERNET_ADDRESS_LENGTH];
	u8 source[NET_ETHERNET_ADDRESS_LENGTH];
	u16 type;
};

static u16 net_be16(u16 value)
{
	return (u16)((value << 8) | (value >> 8));
}

static u32 net_be32(u32 value)
{
	return ((value & 0x000000FF) << 24) |
	       ((value & 0x0000FF00) << 8) |
	       ((value & 0x00FF0000) >> 8) |
	       ((value & 0xFF000000) >> 24);
}

static void net_copy_mac(u8 *destination, const u8 *source)
{
	memcpy(destination, source, NET_ETHERNET_ADDRESS_LENGTH);
}

static int net_mac_equal(const u8 *left, const u8 *right)
{
	for (u32 index = 0; index < NET_ETHERNET_ADDRESS_LENGTH; index++)
		if (left[index] != right[index]) return 0;
	return 1;
}

static int net_ipv4_equal(const u8 *left, const u8 *right)
{
	for (u32 index = 0; index < 4; index++)
		if (left[index] != right[index]) return 0;
	return 1;
}
