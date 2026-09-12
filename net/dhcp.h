#define NET_DHCP_CLIENT_PORT 68
#define NET_DHCP_SERVER_PORT 67
#define NET_DHCP_FIXED_LENGTH 236
#define NET_DHCP_MAGIC_COOKIE 0x63825363
#define NET_DHCP_DISCOVER 1
#define NET_DHCP_OFFER 2
#define NET_DHCP_REQUEST 3
#define NET_DHCP_ACK 5
#define NET_DHCP_NAK 6
#define NET_DHCP_STATE_RENEWING 4
#define NET_DHCP_STATE_EXPIRED 6
#define NET_DHCP_STATE_REBINDING 7
#define NET_DHCP_OPTION_SUBNET_MASK 1
#define NET_DHCP_OPTION_ROUTER 3
#define NET_DHCP_OPTION_LEASE_TIME 51
#define NET_DHCP_OPTION_MESSAGE_TYPE 53
#define NET_DHCP_OPTION_SERVER_IDENTIFIER 54
#define NET_DHCP_OPTION_PARAMETER_LIST 55
#define NET_DHCP_OPTION_REQUESTED_IP 50
#define NET_DHCP_OPTION_CLIENT_IDENTIFIER 61
#define NET_DHCP_NO_PACKET 1
#define NET_DHCP_OFFER_RECEIVED 2
#define NET_DHCP_BOUND 3

__attribute__((packed)) struct net_dhcp_header
{
	u8 operation;
	u8 hardware_type;
	u8 hardware_length;
	u8 hops;
	u32 transaction_id;
	u16 seconds;
	u16 flags;
	u8 client_ip[4];
	u8 your_ip[4];
	u8 server_ip[4];
	u8 relay_ip[4];
	u8 client_hardware[16];
	u8 server_name[64];
	u8 boot_file[128];
};

struct net_dhcp_state
{
	u32 transaction_id;
	u8 client_mac[NET_ETHERNET_ADDRESS_LENGTH];
	u8 offered_ip[4];
	u8 server_ip[4];
	u8 subnet_mask[4];
	u8 gateway[4];
	u32 lease_time;
	u32 state;
	u32 retries;
	u32 bound;
	u32 lease_started;
	u32 renew_at;
	u32 rebind_at;
	u32 expire_at;
	u32 renewal_count;
};

static void net_dhcp_init(struct net_dhcp_state *state, const u8 *mac,
                          u32 transaction_id)
{
	if (!state) return;
	memset(state, 0, sizeof(*state));
	if (mac) net_copy_mac(state->client_mac, mac);
	state->transaction_id = transaction_id ? transaction_id : 0x5A49524F;
}

static void net_dhcp_set_lease(struct net_dhcp_state *state, u32 now,
                               u32 ticks_per_second)
{
	if (!state || !ticks_per_second) return;
	u32 lease_seconds = state->lease_time ? state->lease_time : 3600;
	u64 lease_ticks = (u64)lease_seconds * ticks_per_second;
	if (lease_ticks > 0xFFFFFFFFu) lease_ticks = 0xFFFFFFFFu;
	u32 duration = (u32)lease_ticks;
	u32 renew = duration / 2;
	u32 rebind = (u32)(((u64)duration * 7) / 8);
	if (!renew) renew = 1;
	if (!rebind) rebind = 1;
	state->lease_started = now;
	state->renew_at = now + renew;
	state->rebind_at = now + rebind;
	state->expire_at = now + duration;
}

static int net_dhcp_add_option(u8 *options, u32 capacity, u32 *offset,
                               u8 code, u8 length, const void *data)
{
	if (!options || !offset || !data || *offset > capacity ||
	    capacity - *offset < (u32)length + 2) return -1;
	options[(*offset)++] = code;
	options[(*offset)++] = length;
	memcpy(options + *offset, data, length);
	*offset += length;
	return 0;
}

static int net_dhcp_build_packet(struct net_dhcp_state *state, u8 message,
                                 u8 *payload, u32 capacity)
{
	if (!state || !payload || capacity < NET_DHCP_FIXED_LENGTH + 5)
		return -1;
	memset(payload, 0, capacity);
	struct net_dhcp_header *packet =
		(struct net_dhcp_header *)payload;
	packet->operation = 1;
	packet->hardware_type = 1;
	packet->hardware_length = NET_ETHERNET_ADDRESS_LENGTH;
	packet->transaction_id = net_be32(state->transaction_id);
	packet->flags = net_be16(0x8000);
	net_copy_mac(packet->client_hardware, state->client_mac);
	u8 *options = payload + NET_DHCP_FIXED_LENGTH;
	u32 offset = 0;
	u32 cookie = net_be32(NET_DHCP_MAGIC_COOKIE);
	memcpy(options + offset, &cookie, sizeof(cookie));
	offset += sizeof(cookie);
	if (net_dhcp_add_option(options, capacity - NET_DHCP_FIXED_LENGTH,
			&offset, NET_DHCP_OPTION_MESSAGE_TYPE, 1, &message)) return -2;
	u8 client_identifier[7] = {1, 0, 0, 0, 0, 0, 0};
	memcpy(client_identifier + 1, state->client_mac, 6);
	if (net_dhcp_add_option(options, capacity - NET_DHCP_FIXED_LENGTH,
			&offset, NET_DHCP_OPTION_CLIENT_IDENTIFIER, 7,
			client_identifier)) return -3;
	if (message == NET_DHCP_REQUEST) {
		if (net_dhcp_add_option(options, capacity - NET_DHCP_FIXED_LENGTH,
				&offset, NET_DHCP_OPTION_REQUESTED_IP, 4,
				state->offered_ip)) return -4;
		if (net_dhcp_add_option(options, capacity - NET_DHCP_FIXED_LENGTH,
				&offset, NET_DHCP_OPTION_SERVER_IDENTIFIER, 4,
				state->server_ip)) return -5;
	}
	u8 parameters[4] = {NET_DHCP_OPTION_SUBNET_MASK,
		NET_DHCP_OPTION_ROUTER, 6, NET_DHCP_OPTION_LEASE_TIME};
	if (net_dhcp_add_option(options, capacity - NET_DHCP_FIXED_LENGTH,
			&offset, NET_DHCP_OPTION_PARAMETER_LIST, 4, parameters)) return -6;
	if (offset >= capacity - NET_DHCP_FIXED_LENGTH) return -7;
	options[offset++] = 255;
	return NET_DHCP_FIXED_LENGTH + offset;
}

static int net_dhcp_send_message(struct net_dhcp_state *state,
                                 struct net_arp_state *arp,
                                 struct net_udp_state *udp,
                                 u8 message,
                                 net_send_frame_function send)
{
	static const u8 broadcast_mac[NET_ETHERNET_ADDRESS_LENGTH] =
		{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	static const u8 zero_ip[4] = {0, 0, 0, 0};
	static const u8 broadcast_ip[4] = {255, 255, 255, 255};
	u8 payload[NET_UDP_PAYLOAD_MAX];
	int length = net_dhcp_build_packet(state, message, payload, sizeof(payload));
	if (length < 0) return length;
	return net_udp_send_frame(udp, arp, zero_ip, broadcast_ip, broadcast_mac,
	                          NET_DHCP_CLIENT_PORT, NET_DHCP_SERVER_PORT,
	                          payload, length, send);
}

static int net_dhcp_start(struct net_dhcp_state *state,
                          struct net_arp_state *arp,
                          struct net_udp_state *udp,
                          net_send_frame_function send)
{
	if (!state || !arp || !udp || !send) return -1;
	int result = net_udp_bind(udp, NET_DHCP_CLIENT_PORT);
	if (result) return result;
	state->state = NET_DHCP_DISCOVER;
	state->retries++;
	return net_dhcp_send_message(state, arp, udp, NET_DHCP_DISCOVER, send);
}

static int net_dhcp_find_option(const struct net_udp_datagram *datagram,
                                u8 wanted, const u8 **data, u8 *length)
{
	if (!datagram || !data || !length ||
	    datagram->length < NET_DHCP_FIXED_LENGTH + 4) return -1;
	const u8 *payload = datagram->payload;
	u32 cookie = 0;
	memcpy(&cookie, payload + NET_DHCP_FIXED_LENGTH, sizeof(cookie));
	if (cookie != net_be32(NET_DHCP_MAGIC_COOKIE)) return -2;
	u32 offset = NET_DHCP_FIXED_LENGTH + 4;
	while (offset < datagram->length) {
		u8 code = payload[offset++];
		if (code == 0) continue;
		if (code == 255) break;
		if (offset >= datagram->length) return -3;
		u8 option_length = payload[offset++];
		if (option_length > datagram->length - offset) return -4;
		if (code == wanted) {
			*data = payload + offset;
			*length = option_length;
			return 0;
		}
		offset += option_length;
	}
	return 1;
}

static void net_dhcp_copy_option(const struct net_udp_datagram *datagram,
                                 u8 option, u8 *destination, u8 length)
{
	const u8 *data = 0;
	u8 option_length = 0;
	if (!net_dhcp_find_option(datagram, option, &data, &option_length) &&
	    option_length == length)
		memcpy(destination, data, length);
}

static int net_dhcp_handle(struct net_dhcp_state *state,
                           struct net_arp_state *arp,
                           struct net_ipv4_state *ipv4,
                           struct net_udp_state *udp,
                           const struct net_udp_datagram *datagram,
                           u32 now, u32 ticks_per_second,
                           net_send_frame_function send)
{
	if (!state || !arp || !ipv4 || !udp || !datagram || !send ||
	    datagram->length < NET_DHCP_FIXED_LENGTH + 4) return -1;
	const struct net_dhcp_header *packet =
		(const struct net_dhcp_header *)datagram->payload;
	if (packet->operation != 2 || packet->hardware_type != 1 ||
	    packet->hardware_length != NET_ETHERNET_ADDRESS_LENGTH ||
	    net_be32(packet->transaction_id) != state->transaction_id ||
	    !net_mac_equal(packet->client_hardware, state->client_mac)) return -2;
	const u8 *message_data = 0;
	u8 message_length = 0;
	if (net_dhcp_find_option(datagram, NET_DHCP_OPTION_MESSAGE_TYPE,
			&message_data, &message_length) || message_length != 1) return -3;
	u8 message = message_data[0];
	if (message == NET_DHCP_NAK) return -4;
	if (message != NET_DHCP_OFFER && message != NET_DHCP_ACK) return 0;
	net_dhcp_copy_option(datagram, NET_DHCP_OPTION_SERVER_IDENTIFIER,
	                     state->server_ip, 4);
	net_dhcp_copy_option(datagram, NET_DHCP_OPTION_SUBNET_MASK,
	                     state->subnet_mask, 4);
	net_dhcp_copy_option(datagram, NET_DHCP_OPTION_ROUTER,
	                     state->gateway, 4);
	const u8 *lease_data = 0;
	u8 lease_length = 0;
	if (!net_dhcp_find_option(datagram, NET_DHCP_OPTION_LEASE_TIME,
			&lease_data, &lease_length) && lease_length == 4) {
		u32 lease = 0;
		memcpy(&lease, lease_data, 4);
		state->lease_time = net_be32(lease);
	}
	if (message == NET_DHCP_OFFER) {
		const u8 *server_data = 0;
		u8 server_length = 0;
		if (net_dhcp_find_option(datagram,
				NET_DHCP_OPTION_SERVER_IDENTIFIER, &server_data,
				&server_length) || server_length != 4) return -7;
		memcpy(state->server_ip, server_data, 4);
		memcpy(state->offered_ip, packet->your_ip, 4);
		state->state = NET_DHCP_OFFER;
		int result = net_dhcp_send_message(state, arp, udp,
		                                   NET_DHCP_REQUEST, send);
		if (result) return -5;
		state->state = NET_DHCP_REQUEST;
		return NET_DHCP_OFFER_RECEIVED;
	}
	if (state->state != NET_DHCP_REQUEST &&
	    state->state != NET_DHCP_STATE_RENEWING &&
	    state->state != NET_DHCP_STATE_REBINDING) return -6;
	memcpy(state->offered_ip, packet->your_ip, 4);
	memcpy(arp->local_ip, state->offered_ip, 4);
	memcpy(ipv4->local_ip, state->offered_ip, 4);
	net_arp_invalidate(arp, state->server_ip);
	state->bound = 1;
	state->state = NET_DHCP_ACK;
	state->renewal_count = 0;
	net_dhcp_set_lease(state, now, ticks_per_second);
	return NET_DHCP_BOUND;
}

static int net_dhcp_tick(struct net_dhcp_state *state,
                         struct net_arp_state *arp,
                         struct net_ipv4_state *ipv4,
                         struct net_udp_state *udp,
                         u32 now, net_send_frame_function send)
{
	if (!state || !arp || !ipv4 || !udp || !send || !state->bound) return 0;
	if ((int)(now - state->expire_at) >= 0) {
		state->bound = 0;
		state->state = NET_DHCP_STATE_EXPIRED;
		memset(arp->local_ip, 0, 4);
		memset(ipv4->local_ip, 0, 4);
		net_arp_clear_cache(arp);
		return -1;
	}
	if (state->state == NET_DHCP_ACK &&
	    (int)(now - state->renew_at) >= 0) {
		state->state = NET_DHCP_STATE_RENEWING;
		state->renewal_count++;
		return net_dhcp_send_message(state, arp, udp,
		                             NET_DHCP_REQUEST, send);
	}
	if (state->state == NET_DHCP_STATE_RENEWING &&
	    (int)(now - state->rebind_at) >= 0) {
		state->state = NET_DHCP_STATE_REBINDING;
		return net_dhcp_send_message(state, arp, udp,
		                             NET_DHCP_REQUEST, send);
	}
	return 0;
}

static int net_dhcp_poll(struct net_dhcp_state *state,
                         struct net_arp_state *arp,
                         struct net_ipv4_state *ipv4,
                         struct net_udp_state *udp,
                         u32 now, u32 ticks_per_second,
                         net_send_frame_function send)
{
	struct net_udp_datagram datagram;
	int result = net_udp_receive(udp, NET_DHCP_CLIENT_PORT, &datagram);
	if (result == 1) return NET_DHCP_NO_PACKET;
	if (result) return result;
	return net_dhcp_handle(state, arp, ipv4, udp, &datagram, now,
	                       ticks_per_second, send);
}
