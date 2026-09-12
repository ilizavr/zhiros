#define NET_IP_PROTOCOL_UDP 17
#define NET_UDP_HEADER_LENGTH 8
#define NET_UDP_PAYLOAD_MAX 512
#define NET_UDP_ECHO_PORT 7
#define NET_UDP_BROADCAST_PORT 68
#define NET_UDP_SOCKET_COUNT 4
#define NET_UDP_QUEUE_LENGTH 4
#define NET_NETWORK_POLL_UDP_REPLIED 5
#define NET_NETWORK_POLL_UDP_QUEUED 6
#define NET_NETWORK_POLL_UDP_DROPPED 7

__attribute__((packed)) struct net_udp_header
{
	u16 source_port;
	u16 destination_port;
	u16 length;
	u16 checksum;
};

struct net_udp_datagram
{
	u8 source_ip[4];
	u16 source_port;
	u16 length;
	u8 payload[NET_UDP_PAYLOAD_MAX];
};

struct net_udp_socket
{
	u16 port;
	u32 bound;
	u32 head;
	u32 tail;
	u32 count;
	struct net_udp_datagram queue[NET_UDP_QUEUE_LENGTH];
};

struct net_udp_state
{
	u16 port;
	u32 sent;
	u32 received;
	u32 replied;
	u32 queued;
	u32 drops;
	struct net_udp_socket sockets[NET_UDP_SOCKET_COUNT];
};

static struct net_udp_socket *net_udp_find_socket(struct net_udp_state *state,
                                                   u16 port)
{
	if (!state || !port) return 0;
	for (u32 index = 0; index < NET_UDP_SOCKET_COUNT; index++)
		if (state->sockets[index].bound &&
		    state->sockets[index].port == port)
			return &state->sockets[index];
	return 0;
}

static int net_udp_bind(struct net_udp_state *state, u16 port)
{
	if (!state || !port || port == state->port) return -1;
	if (net_udp_find_socket(state, port)) return 0;
	for (u32 index = 0; index < NET_UDP_SOCKET_COUNT; index++) {
		if (state->sockets[index].bound) continue;
		memset(&state->sockets[index], 0, sizeof(state->sockets[index]));
		state->sockets[index].port = port;
		state->sockets[index].bound = 1;
		return 0;
	}
	return -2;
}

static int net_udp_unbind(struct net_udp_state *state, u16 port)
{
	struct net_udp_socket *socket = net_udp_find_socket(state, port);
	if (!socket) return -1;
	memset(socket, 0, sizeof(*socket));
	return 0;
}

static int net_udp_receive(struct net_udp_state *state, u16 port,
                           struct net_udp_datagram *datagram)
{
	if (!datagram) return -1;
	struct net_udp_socket *socket = net_udp_find_socket(state, port);
	if (!socket) return -2;
	if (!socket->count) return 1;
	*datagram = socket->queue[socket->head];
	memset(&socket->queue[socket->head], 0,
	       sizeof(socket->queue[socket->head]));
	socket->head = (socket->head + 1) % NET_UDP_QUEUE_LENGTH;
	socket->count--;
	return 0;
}

static void net_udp_init(struct net_udp_state *state, u16 port)
{
	if (!state) return;
	memset(state, 0, sizeof(*state));
	state->port = port;
}

static u16 net_udp_checksum(const u8 *source_ip, const u8 *destination_ip,
                            const void *data, u16 length)
{
	const u8 *bytes = (const u8 *)data;
	u32 sum = 0;
	for (u32 index = 0; index < 4; index += 2) {
		sum += ((u16)source_ip[index] << 8) | source_ip[index + 1];
		sum += ((u16)destination_ip[index] << 8) | destination_ip[index + 1];
	}
	sum += NET_IP_PROTOCOL_UDP;
	sum += length;
	while (length > 1) {
		sum += ((u16)bytes[0] << 8) | bytes[1];
		bytes += 2;
		length -= 2;
	}
	if (length) sum += (u16)bytes[0] << 8;
	while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
	return (u16)~sum;
}

static int net_udp_send_frame(struct net_udp_state *state,
                               struct net_arp_state *arp,
                               const u8 *source_ip,
                               const u8 *destination_ip,
                               const u8 *destination_mac,
                               u16 source_port,
                               u16 destination_port,
                               const void *payload,
                               u16 payload_length,
                               net_send_frame_function send)
{
	if (!state || !arp || !source_ip || !destination_ip ||
	    !destination_mac || !source_port || !send ||
	    payload_length > NET_UDP_PAYLOAD_MAX ||
	    (payload_length && !payload)) return -1;
	u8 frame[NET_ETHERNET_HEADER_LENGTH + NET_IPV4_HEADER_LENGTH +
	         NET_UDP_HEADER_LENGTH + NET_UDP_PAYLOAD_MAX];
	memset(frame, 0, sizeof(frame));
	struct net_ethernet_header *ethernet =
		(struct net_ethernet_header *)frame;
	struct net_ipv4_header *ip =
		(struct net_ipv4_header *)(frame + NET_ETHERNET_HEADER_LENGTH);
	struct net_udp_header *udp =
		(struct net_udp_header *)(frame + NET_ETHERNET_HEADER_LENGTH +
		                          NET_IPV4_HEADER_LENGTH);
	net_copy_mac(ethernet->destination, destination_mac);
	net_copy_mac(ethernet->source, arp->local_mac);
	ethernet->type = net_be16(NET_ETHERTYPE_IPV4);
	u16 udp_length = NET_UDP_HEADER_LENGTH + payload_length;
	u16 ip_length = NET_IPV4_HEADER_LENGTH + udp_length;
	ip->version_ihl = (NET_IPV4_VERSION << 4) | 5;
	ip->total_length = net_be16(ip_length);
	ip->identification = 0;
	ip->flags_fragment = 0;
	ip->ttl = NET_IPV4_TTL;
	ip->protocol = NET_IP_PROTOCOL_UDP;
	ip->checksum = 0;
	memcpy(ip->source, source_ip, 4);
	memcpy(ip->destination, destination_ip, 4);
	ip->checksum = net_be16(net_checksum(ip, NET_IPV4_HEADER_LENGTH));
	udp->source_port = net_be16(source_port);
	udp->destination_port = net_be16(destination_port);
	udp->length = net_be16(udp_length);
	udp->checksum = 0;
	if (payload_length)
		memcpy((u8 *)udp + NET_UDP_HEADER_LENGTH, payload, payload_length);
	udp->checksum = net_be16(net_udp_checksum(
		ip->source, ip->destination, udp, udp_length));
	if (!udp->checksum) udp->checksum = 0xFFFF;
	u32 frame_length = NET_ETHERNET_HEADER_LENGTH + ip_length;
	if (frame_length < NET_ETHERNET_MIN_FRAME_LENGTH)
		frame_length = NET_ETHERNET_MIN_FRAME_LENGTH;
	int result = send(frame, frame_length);
	if (!result) state->sent++;
	return result;
}

static int net_udp_send_from(struct net_udp_state *state,
                              struct net_arp_state *arp,
                              u16 source_port,
                              const u8 *destination_ip,
                              u16 destination_port,
                              const void *payload,
                              u16 payload_length,
                              net_send_frame_function send)
{
	if (!state || !arp || !destination_ip || !send) return -1;
	u8 destination_mac[NET_ETHERNET_ADDRESS_LENGTH];
	int lookup = net_arp_lookup(arp, destination_ip, destination_mac);
	if (lookup == 1) {
		int result = net_arp_request(arp, destination_ip, send);
		return result ? result : 1;
	}
	if (lookup) return lookup;
	return net_udp_send_frame(state, arp, arp->local_ip, destination_ip,
	                          destination_mac, source_port, destination_port,
	                          payload, payload_length, send);
}

static int net_udp_send(struct net_udp_state *state,
                        struct net_arp_state *arp,
                        const u8 *destination_ip,
                        u16 destination_port,
                        const void *payload,
                        u16 payload_length,
                        net_send_frame_function send)
{
	if (!state) return -1;
	return net_udp_send_from(state, arp, state->port, destination_ip,
	                         destination_port, payload, payload_length, send);
}

static int net_udp_handle_frame(struct net_udp_state *state,
                                struct net_arp_state *arp,
                                u8 *frame, u32 length,
                                net_send_frame_function send)
{
	if (!state || !arp || !frame || !send ||
	    length < NET_ETHERNET_HEADER_LENGTH + NET_IPV4_HEADER_LENGTH)
		return -1;
	struct net_ethernet_header *ethernet =
		(struct net_ethernet_header *)frame;
	if (net_be16(ethernet->type) != NET_ETHERTYPE_IPV4) return 0;
	struct net_ipv4_header *ip =
		(struct net_ipv4_header *)(frame + NET_ETHERNET_HEADER_LENGTH);
	u8 version = ip->version_ihl >> 4;
	u32 header_length = (ip->version_ihl & 0x0F) * 4;
	u16 total_length = net_be16(ip->total_length);
	if (version != NET_IPV4_VERSION || header_length < NET_IPV4_HEADER_LENGTH ||
	    total_length < header_length ||
	    total_length > length - NET_ETHERNET_HEADER_LENGTH ||
	    (net_be16(ip->flags_fragment) & 0x3FFF) ||
	    net_checksum(ip, header_length) != 0) return -2;
	if (ip->protocol != NET_IP_PROTOCOL_UDP) return 0;
	if (total_length < header_length + NET_UDP_HEADER_LENGTH) return -3;
	struct net_udp_header *udp =
		(struct net_udp_header *)(frame + NET_ETHERNET_HEADER_LENGTH +
		                          header_length);
	u16 udp_length = net_be16(udp->length);
	if (udp_length < NET_UDP_HEADER_LENGTH ||
	    udp_length > total_length - header_length) return -4;
	if (udp->checksum && net_udp_checksum(
			ip->source, ip->destination, udp, udp_length)) return -5;
	u16 destination_port = net_be16(udp->destination_port);
	static const u8 broadcast_ip[4] = {255, 255, 255, 255};
	if (!net_ipv4_equal(ip->destination, arp->local_ip) &&
	    !(destination_port == NET_UDP_BROADCAST_PORT &&
	      net_ipv4_equal(ip->destination, broadcast_ip))) return 0;
	net_arp_cache_update(arp, ip->source, ethernet->source);
	if (destination_port != state->port) {
		struct net_udp_socket *socket =
			net_udp_find_socket(state, destination_port);
		if (!socket) return 0;
		u16 payload_length = udp_length - NET_UDP_HEADER_LENGTH;
		if (payload_length > NET_UDP_PAYLOAD_MAX ||
		    socket->count >= NET_UDP_QUEUE_LENGTH) {
			state->drops++;
			return NET_NETWORK_POLL_UDP_DROPPED;
		}
		struct net_udp_datagram *datagram = &socket->queue[socket->tail];
		memcpy(datagram->source_ip, ip->source, 4);
		datagram->source_port = net_be16(udp->source_port);
		datagram->length = payload_length;
		if (payload_length)
			memcpy(datagram->payload,
			       (u8 *)udp + NET_UDP_HEADER_LENGTH, payload_length);
		socket->tail = (socket->tail + 1) % NET_UDP_QUEUE_LENGTH;
		socket->count++;
		state->received++;
		state->queued++;
		return NET_NETWORK_POLL_UDP_QUEUED;
	}
	u8 old_source_mac[NET_ETHERNET_ADDRESS_LENGTH];
	u8 old_source_ip[4];
	net_copy_mac(old_source_mac, ethernet->source);
	memcpy(old_source_ip, ip->source, 4);
	u16 old_source_port = udp->source_port;
	net_copy_mac(ethernet->destination, old_source_mac);
	net_copy_mac(ethernet->source, arp->local_mac);
	memcpy(ip->destination, old_source_ip, 4);
	memcpy(ip->source, arp->local_ip, 4);
	udp->source_port = udp->destination_port;
	udp->destination_port = old_source_port;
	udp->checksum = 0;
	udp->checksum = net_be16(net_udp_checksum(
		ip->source, ip->destination, udp, udp_length));
	if (!udp->checksum) udp->checksum = 0xFFFF;
	ip->checksum = 0;
	ip->checksum = net_be16(net_checksum(ip, header_length));
	u32 frame_length = NET_ETHERNET_HEADER_LENGTH + total_length;
	if (frame_length < NET_ETHERNET_MIN_FRAME_LENGTH)
		frame_length = NET_ETHERNET_MIN_FRAME_LENGTH;
	int result = send(frame, frame_length);
	if (result) return -6;
	state->received++;
	state->replied++;
	return NET_NETWORK_POLL_UDP_REPLIED;
}

static int net_network_poll(struct net_ipv4_state *ipv4,
                            struct net_udp_state *udp,
                            struct net_tcp_state *tcp,
                            struct net_arp_state *arp,
                            net_receive_frame_function receive,
                            net_send_frame_function send)
{
	static u8 frame[NET_PACKET_BUFFER_SIZE];
	u32 length = 0;
	if (!ipv4 || !udp || !tcp || !arp || !receive || !send) return -1;
	int result = receive(frame, sizeof(frame), &length);
	if (result == 1) return 0;
	if (result < 0) return result;
	result = net_arp_handle_frame(arp, frame, length, send);
	if (result == 1) return NET_IPV4_POLL_ARP_REPLY;
	if (result == 2) return NET_IPV4_POLL_ARP_REQUEST_REPLIED;
	if (result < 0) return result;
	result = net_ipv4_handle_frame(ipv4, arp, frame, length, send);
	if (result) return result;
	result = net_udp_handle_frame(udp, arp, frame, length, send);
	if (result) return result;
	return net_tcp_handle_frame(tcp, arp, frame, length, send);
}
