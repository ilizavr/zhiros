#define NET_ARP_CACHE_SIZE 4
#define NET_ARP_CACHE_TTL 3000
#define NET_ARP_HARDWARE_ETHERNET 1
#define NET_ARP_OPERATION_REQUEST 1
#define NET_ARP_OPERATION_REPLY 2

__attribute__((packed)) struct net_arp_packet
{
	struct net_ethernet_header ethernet;
	u16 hardware_type;
	u16 protocol_type;
	u8 hardware_length;
	u8 protocol_length;
	u16 operation;
	u8 sender_hardware[NET_ETHERNET_ADDRESS_LENGTH];
	u8 sender_protocol[4];
	u8 target_hardware[NET_ETHERNET_ADDRESS_LENGTH];
	u8 target_protocol[4];
};

struct net_arp_cache_entry
{
	u8 ip[4];
	u8 mac[NET_ETHERNET_ADDRESS_LENGTH];
	u32 valid;
	u32 age;
};

struct net_arp_state
{
	u8 local_mac[NET_ETHERNET_ADDRESS_LENGTH];
	u8 local_ip[4];
	struct net_arp_cache_entry cache[NET_ARP_CACHE_SIZE];
	u32 requests_sent;
	u32 replies_received;
	u32 replies_sent;
	u32 cache_hits;
	u32 cache_misses;
	u32 cache_expired;
};

static void net_arp_init(struct net_arp_state *state, const u8 *mac,
                         const u8 *ip)
{
	if (!state) return;
	memset(state, 0, sizeof(*state));
	if (mac) net_copy_mac(state->local_mac, mac);
	if (ip) memcpy(state->local_ip, ip, 4);
}

static void net_arp_cache_update(struct net_arp_state *state,
                                 const u8 *ip, const u8 *mac)
{
	if (!state || !ip || !mac) return;
	struct net_arp_cache_entry *entry = 0;
	for (u32 index = 0; index < NET_ARP_CACHE_SIZE; index++) {
		if (state->cache[index].valid &&
		    net_ipv4_equal(state->cache[index].ip, ip)) {
			entry = &state->cache[index];
			break;
		}
	}
	if (!entry) {
		for (u32 index = 0; index < NET_ARP_CACHE_SIZE; index++) {
			if (!state->cache[index].valid) {
				entry = &state->cache[index];
				break;
			}
		}
	}
	if (!entry) entry = &state->cache[0];
	memcpy(entry->ip, ip, 4);
	net_copy_mac(entry->mac, mac);
	entry->valid = 1;
	entry->age = 0;
}

static int net_arp_lookup(struct net_arp_state *state,
                          const u8 *ip, u8 *mac)
{
	if (!state || !ip || !mac) return -1;
	for (u32 index = 0; index < NET_ARP_CACHE_SIZE; index++) {
		if (!state->cache[index].valid ||
		    !net_ipv4_equal(state->cache[index].ip, ip)) continue;
		if (state->cache[index].age >= NET_ARP_CACHE_TTL) {
			state->cache[index].valid = 0;
			state->cache_expired++;
			break;
		}
		net_copy_mac(mac, state->cache[index].mac);
		state->cache_hits++;
		return 0;
	}
	state->cache_misses++;
	return 1;
}

static void net_arp_tick(struct net_arp_state *state)
{
	if (!state) return;
	for (u32 index = 0; index < NET_ARP_CACHE_SIZE; index++) {
		if (state->cache[index].valid &&
		    state->cache[index].age < NET_ARP_CACHE_TTL)
			state->cache[index].age++;
	}
}

static void net_arp_invalidate(struct net_arp_state *state, const u8 *ip)
{
	if (!state || !ip) return;
	for (u32 index = 0; index < NET_ARP_CACHE_SIZE; index++) {
		if (state->cache[index].valid &&
		    net_ipv4_equal(state->cache[index].ip, ip)) {
			state->cache[index].valid = 0;
			return;
		}
	}
}

static void net_arp_clear_cache(struct net_arp_state *state)
{
	if (!state) return;
	memset(state->cache, 0, sizeof(state->cache));
}

static int net_arp_send_packet(struct net_arp_state *state,
                               const u8 *destination,
                               u16 operation,
                               const u8 *sender_mac,
                               const u8 *sender_ip,
                               const u8 *target_mac,
                               const u8 *target_ip,
                               net_send_frame_function send)
{
	if (!state || !destination || !sender_mac || !sender_ip || !target_mac ||
	    !target_ip || !send) return -1;
	u8 frame[NET_ETHERNET_MIN_FRAME_LENGTH];
	memset(frame, 0, sizeof(frame));
	struct net_arp_packet *packet = (struct net_arp_packet *)frame;
	net_copy_mac(packet->ethernet.destination, destination);
	net_copy_mac(packet->ethernet.source, sender_mac);
	packet->ethernet.type = net_be16(NET_ETHERTYPE_ARP);
	packet->hardware_type = net_be16(NET_ARP_HARDWARE_ETHERNET);
	packet->protocol_type = net_be16(NET_ETHERTYPE_IPV4);
	packet->hardware_length = NET_ETHERNET_ADDRESS_LENGTH;
	packet->protocol_length = 4;
	packet->operation = net_be16(operation);
	net_copy_mac(packet->sender_hardware, sender_mac);
	memcpy(packet->sender_protocol, sender_ip, 4);
	net_copy_mac(packet->target_hardware, target_mac);
	memcpy(packet->target_protocol, target_ip, 4);
	return send(frame, sizeof(frame));
}

static int net_arp_request(struct net_arp_state *state, const u8 *target_ip,
                           net_send_frame_function send)
{
	static const u8 broadcast[NET_ETHERNET_ADDRESS_LENGTH] =
		{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	static const u8 empty_mac[NET_ETHERNET_ADDRESS_LENGTH] = {0, 0, 0, 0, 0, 0};
	int result = net_arp_send_packet(state, broadcast, NET_ARP_OPERATION_REQUEST,
	                                state ? state->local_mac : 0,
	                                state ? state->local_ip : 0, empty_mac,
	                                target_ip, send);
	if (!result && state) state->requests_sent++;
	return result;
}

static int net_arp_handle_frame(struct net_arp_state *state,
                                const void *frame, u32 length,
                                net_send_frame_function send)
{
	if (!state || !frame || length < NET_ETHERNET_HEADER_LENGTH) return -1;
	const struct net_arp_packet *packet =
		(const struct net_arp_packet *)frame;
	if (net_be16(packet->ethernet.type) != NET_ETHERTYPE_ARP) return 0;
	if (length < sizeof(struct net_arp_packet) ||
	    net_be16(packet->hardware_type) != NET_ARP_HARDWARE_ETHERNET ||
	    net_be16(packet->protocol_type) != NET_ETHERTYPE_IPV4 ||
	    packet->hardware_length != NET_ETHERNET_ADDRESS_LENGTH ||
	    packet->protocol_length != 4) return -2;
	net_arp_cache_update(state, packet->sender_protocol,
	                     packet->sender_hardware);
	u16 operation = net_be16(packet->operation);
	if (operation == NET_ARP_OPERATION_REQUEST &&
	    net_ipv4_equal(packet->target_protocol, state->local_ip)) {
		int result = net_arp_send_packet(
			state, packet->sender_hardware, NET_ARP_OPERATION_REPLY,
			state->local_mac, state->local_ip, packet->sender_hardware,
			packet->sender_protocol, send);
		if (result) return -3;
		state->replies_sent++;
		return 2;
	}
	if (operation == NET_ARP_OPERATION_REPLY) {
		state->replies_received++;
		return 1;
	}
	return -2;
}

static int net_arp_poll(struct net_arp_state *state,
                        net_receive_frame_function receive,
                        net_send_frame_function send)
{
	static u8 frame[NET_PACKET_BUFFER_SIZE];
	u32 length = 0;
	if (!state || !receive || !send) return -1;
	int result = receive(frame, sizeof(frame), &length);
	if (result == 1) return 0;
	if (result < 0) return result;
	return net_arp_handle_frame(state, frame, length, send);
}
