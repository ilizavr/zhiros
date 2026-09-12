#define NET_IPV4_HEADER_LENGTH 20
#define NET_IPV4_VERSION 4
#define NET_IPV4_TTL 64
#define NET_IP_PROTOCOL_ICMP 1
#define NET_ICMP_HEADER_LENGTH 8
#define NET_ICMP_ECHO_REPLY 0
#define NET_ICMP_ECHO_REQUEST 8
#define NET_ICMP_PAYLOAD_LENGTH 32
#define NET_IPV4_POLL_ARP_REPLY 3
#define NET_IPV4_POLL_ARP_REQUEST_REPLIED 4

__attribute__((packed)) struct net_ipv4_header
{
	u8 version_ihl;
	u8 dscp_ecn;
	u16 total_length;
	u16 identification;
	u16 flags_fragment;
	u8 ttl;
	u8 protocol;
	u16 checksum;
	u8 source[4];
	u8 destination[4];
};

__attribute__((packed)) struct net_icmp_header
{
	u8 type;
	u8 code;
	u16 checksum;
	u16 identifier;
	u16 sequence;
};

struct net_ipv4_state
{
	u8 local_ip[4];
	u16 identification;
	u16 ping_identifier;
	u16 ping_sequence;
	u32 echo_sent;
	u32 echo_received;
	u32 echo_replied;
	u32 drops;
};

static u16 net_checksum(const void *data, u32 length)
{
	const u8 *bytes = (const u8 *)data;
	u32 sum = 0;
	while (length > 1) {
		sum += ((u16)bytes[0] << 8) | bytes[1];
		bytes += 2;
		length -= 2;
		if (sum > 0xFFFF) sum = (sum & 0xFFFF) + (sum >> 16);
	}
	if (length) sum += (u16)bytes[0] << 8;
	while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
	return (u16)~sum;
}

static void net_ipv4_init(struct net_ipv4_state *state, const u8 *local_ip)
{
	if (!state) return;
	memset(state, 0, sizeof(*state));
	if (local_ip) memcpy(state->local_ip, local_ip, 4);
	state->ping_identifier = 0x5A51;
	state->ping_sequence = 1;
}

static int net_parse_ipv4(const char *text, u8 *address)
{
	if (!text || !address) return -1;
	for (u32 part = 0; part < 4; part++) {
		u32 value = 0;
		u32 digits = 0;
		while (*text >= '0' && *text <= '9') {
			value = value * 10 + (*text - '0');
			if (value > 255) return -2;
			text++;
			digits++;
		}
		if (!digits) return -3;
		address[part] = (u8)value;
		if (part < 3) {
			if (*text != '.') return -4;
			text++;
		} else if (*text) return -5;
	}
	return 0;
}

static int net_parse_u16(const char *text, u16 *value)
{
	if (!text || !value || !*text) return -1;
	u32 result = 0;
	while (*text >= '0' && *text <= '9') {
		result = result * 10 + (*text - '0');
		if (result > 65535) return -2;
		text++;
	}
	if (*text) return -3;
	*value = (u16)result;
	return 0;
}

static int net_ipv4_send_echo(struct net_ipv4_state *state,
                              struct net_arp_state *arp,
                              const u8 *target_ip,
                              net_send_frame_function send)
{
	if (!state || !arp || !target_ip || !send) return -1;
	u8 target_mac[NET_ETHERNET_ADDRESS_LENGTH];
	int lookup = net_arp_lookup(arp, target_ip, target_mac);
	if (lookup == 1) {
		int result = net_arp_request(arp, target_ip, send);
		return result ? result : 1;
	}
	if (lookup) return lookup;
	u8 frame[128];
	memset(frame, 0, sizeof(frame));
	struct net_ethernet_header *ethernet =
		(struct net_ethernet_header *)frame;
	struct net_ipv4_header *ip =
		(struct net_ipv4_header *)(frame + NET_ETHERNET_HEADER_LENGTH);
	struct net_icmp_header *icmp =
		(struct net_icmp_header *)(frame + NET_ETHERNET_HEADER_LENGTH +
		                           NET_IPV4_HEADER_LENGTH);
	net_copy_mac(ethernet->destination, target_mac);
	net_copy_mac(ethernet->source, arp->local_mac);
	ethernet->type = net_be16(NET_ETHERTYPE_IPV4);
	ip->version_ihl = (NET_IPV4_VERSION << 4) | 5;
	ip->total_length = net_be16(NET_IPV4_HEADER_LENGTH +
	                            NET_ICMP_HEADER_LENGTH +
	                            NET_ICMP_PAYLOAD_LENGTH);
	ip->identification = net_be16(state->identification++);
	ip->flags_fragment = 0;
	ip->ttl = NET_IPV4_TTL;
	ip->protocol = NET_IP_PROTOCOL_ICMP;
	ip->checksum = 0;
	memcpy(ip->source, state->local_ip, 4);
	memcpy(ip->destination, target_ip, 4);
	ip->checksum = net_be16(net_checksum(ip, NET_IPV4_HEADER_LENGTH));
	icmp->type = NET_ICMP_ECHO_REQUEST;
	icmp->code = 0;
	icmp->checksum = 0;
	icmp->identifier = net_be16(state->ping_identifier);
	icmp->sequence = net_be16(state->ping_sequence++);
	u8 *payload = (u8 *)icmp + NET_ICMP_HEADER_LENGTH;
	for (u32 index = 0; index < NET_ICMP_PAYLOAD_LENGTH; index++)
		payload[index] = (u8)index;
	icmp->checksum = net_be16(net_checksum(
		icmp, NET_ICMP_HEADER_LENGTH + NET_ICMP_PAYLOAD_LENGTH));
	u32 length = NET_ETHERNET_HEADER_LENGTH +
		NET_IPV4_HEADER_LENGTH + NET_ICMP_HEADER_LENGTH +
		NET_ICMP_PAYLOAD_LENGTH;
	if (length < NET_ETHERNET_MIN_FRAME_LENGTH)
		length = NET_ETHERNET_MIN_FRAME_LENGTH;
	int result = send(frame, length);
	if (!result) state->echo_sent++;
	return result;
}

static int net_ipv4_handle_frame(struct net_ipv4_state *state,
                                 struct net_arp_state *arp,
                                 u8 *frame, u32 length,
                                 net_send_frame_function send)
{
	if (!state || !arp || !frame || !send ||
	    length < NET_ETHERNET_HEADER_LENGTH) return -1;
	struct net_ethernet_header *ethernet =
		(struct net_ethernet_header *)frame;
	if (net_be16(ethernet->type) != NET_ETHERTYPE_IPV4) return 0;
	if (length < NET_ETHERNET_HEADER_LENGTH + NET_IPV4_HEADER_LENGTH)
		return -2;
	struct net_ipv4_header *ip =
		(struct net_ipv4_header *)(frame + NET_ETHERNET_HEADER_LENGTH);
	u8 version = ip->version_ihl >> 4;
	u32 header_length = (ip->version_ihl & 0x0F) * 4;
	u16 total_length = net_be16(ip->total_length);
	if (version != NET_IPV4_VERSION || header_length < NET_IPV4_HEADER_LENGTH ||
	    total_length < header_length ||
	    total_length > length - NET_ETHERNET_HEADER_LENGTH ||
	    (net_be16(ip->flags_fragment) & 0x3FFF) ||
	    net_checksum(ip, header_length) != 0) return -3;
	net_arp_cache_update(arp, ip->source, ethernet->source);
	if (!net_ipv4_equal(ip->destination, state->local_ip)) return 0;
	if (ip->protocol != NET_IP_PROTOCOL_ICMP ||
	    total_length < header_length + NET_ICMP_HEADER_LENGTH) return 0;
	struct net_icmp_header *icmp =
		(struct net_icmp_header *)(frame + NET_ETHERNET_HEADER_LENGTH +
		                           header_length);
	u32 icmp_length = total_length - header_length;
	if (net_checksum(icmp, icmp_length) != 0) return -4;
	if (icmp->type == NET_ICMP_ECHO_REPLY && icmp->code == 0 &&
	    net_be16(icmp->identifier) == state->ping_identifier) {
		state->echo_received++;
		return 1;
	}
	if (icmp->type != NET_ICMP_ECHO_REQUEST || icmp->code != 0) return 0;
	u8 old_source_mac[NET_ETHERNET_ADDRESS_LENGTH];
	u8 old_source_ip[4];
	net_copy_mac(old_source_mac, ethernet->source);
	memcpy(old_source_ip, ip->source, 4);
	net_copy_mac(ethernet->destination, old_source_mac);
	net_copy_mac(ethernet->source, arp->local_mac);
	memcpy(ip->destination, old_source_ip, 4);
	memcpy(ip->source, state->local_ip, 4);
	icmp->type = NET_ICMP_ECHO_REPLY;
	icmp->checksum = 0;
	icmp->checksum = net_be16(net_checksum(icmp, icmp_length));
	ip->checksum = 0;
	ip->checksum = net_be16(net_checksum(ip, header_length));
	u32 frame_length = NET_ETHERNET_HEADER_LENGTH + total_length;
	if (frame_length < NET_ETHERNET_MIN_FRAME_LENGTH)
		frame_length = NET_ETHERNET_MIN_FRAME_LENGTH;
	int result = send(frame, frame_length);
	if (result) return -5;
	state->echo_replied++;
	return 2;
}

static int net_ipv4_poll(struct net_ipv4_state *state,
                         struct net_arp_state *arp,
                         net_receive_frame_function receive,
                         net_send_frame_function send)
{
	static u8 frame[NET_PACKET_BUFFER_SIZE];
	u32 length = 0;
	if (!state || !arp || !receive || !send) return -1;
	int result = receive(frame, sizeof(frame), &length);
	if (result == 1) return 0;
	if (result < 0) return result;
	result = net_arp_handle_frame(arp, frame, length, send);
	if (result == 1) return NET_IPV4_POLL_ARP_REPLY;
	if (result == 2) return NET_IPV4_POLL_ARP_REQUEST_REPLIED;
	if (result < 0) return result;
	return net_ipv4_handle_frame(state, arp, frame, length, send);
}
