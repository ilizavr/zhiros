#define NET_IP_PROTOCOL_TCP 6
#define NET_TCP_HEADER_LENGTH 20
#define NET_TCP_PAYLOAD_MAX 512
#define NET_TCP_LISTENER_COUNT 4
#define NET_TCP_CONNECTION_COUNT 4
#define NET_TCP_TX_SEGMENT_COUNT 4
#define NET_TCP_REORDER_SEGMENT_COUNT 4
#define NET_TCP_RECV_BUFFER_SIZE 512
#define NET_TCP_RETRANSMIT_TICKS 250
#define NET_TCP_PERSIST_TICKS 500
#define NET_TCP_MAX_RETRANSMITS 3
#define NET_TCP_IDLE_TIMEOUT_TICKS 30000
#define NET_TCP_TIME_WAIT_TICKS 1000
#define NET_TCP_OPTIONS_MAX_LENGTH 40
#define NET_TCP_DEFAULT_MSS 512
#define NET_TCP_DEFAULT_WINDOW_SCALE 2
#define NET_TCP_FLAG_FIN 0x001
#define NET_TCP_FLAG_SYN 0x002
#define NET_TCP_FLAG_RST 0x004
#define NET_TCP_FLAG_PSH 0x008
#define NET_TCP_FLAG_ACK 0x010
#define NET_TCP_STATE_CLOSED 0
#define NET_TCP_STATE_LISTEN 1
#define NET_TCP_STATE_SYN_SENT 2
#define NET_TCP_STATE_SYN_RECEIVED 3
#define NET_TCP_STATE_ESTABLISHED 4
#define NET_TCP_STATE_FIN_WAIT_1 5
#define NET_TCP_STATE_FIN_WAIT_2 6
#define NET_TCP_STATE_CLOSING 7
#define NET_TCP_STATE_CLOSE_WAIT 8
#define NET_TCP_STATE_LAST_ACK 9
#define NET_TCP_STATE_TIME_WAIT 10
#define NET_NETWORK_POLL_TCP_ESTABLISHED 8
#define NET_NETWORK_POLL_TCP_DATA 9
#define NET_NETWORK_POLL_TCP_CLOSED 10
#define NET_NETWORK_POLL_TCP_FIN 11
#define NET_TCP_OPTION_EOL 0
#define NET_TCP_OPTION_NOP 1
#define NET_TCP_OPTION_MSS 2
#define NET_TCP_OPTION_WINDOW_SCALE 3
#define NET_TCP_OPTION_SACK_PERMITTED 4
#define NET_TCP_OPTION_SACK 5

__attribute__((packed)) struct net_tcp_header
{
	u16 source_port;
	u16 destination_port;
	u32 sequence;
	u32 acknowledgement;
	u16 data_offset_flags;
	u16 window;
	u16 checksum;
	u16 urgent;
};

struct net_tcp_listener
{
	u16 port;
	u32 bound;
};

struct net_tcp_tx_segment
{
	u32 used;
	u16 flags;
	u16 length;
	u32 sequence;
	u32 acknowledgement;
	u32 retransmit_at;
	u32 retransmit_count;
	u32 sacked;
	u8 payload[NET_TCP_PAYLOAD_MAX];
};

struct net_tcp_reorder_segment
{
	u32 used;
	u32 sequence;
	u16 length;
	u8 payload[NET_TCP_PAYLOAD_MAX];
};

struct net_tcp_connection
{
	u32 used;
	u32 state;
	u16 local_port;
	u16 remote_port;
	u16 local_mss;
	u16 remote_mss;
	u32 remote_window;
	u8 local_window_scale;
	u8 remote_window_scale;
	u8 window_scale_enabled;
	u8 sack_permitted;
	u32 congestion_window;
	u32 slow_start_threshold;
	u8 remote_ip[4];
	u8 remote_mac[NET_ETHERNET_ADDRESS_LENGTH];
	u32 send_sequence;
	u32 receive_sequence;
	u32 last_activity;
	u32 persist_at;
	u32 persist_count;
	u32 time_wait_at;
	u32 last_acknowledgement;
	u32 duplicate_ack_count;
	u32 retransmissions;
	u32 fast_retransmits;
	u32 sack_blocks;
	u32 persist_probes;
	u32 tx_count;
	u32 reorder_count;
	struct net_tcp_tx_segment tx[NET_TCP_TX_SEGMENT_COUNT];
	struct net_tcp_reorder_segment reorder[NET_TCP_REORDER_SEGMENT_COUNT];
	u32 receive_length;
	u8 receive_buffer[NET_TCP_RECV_BUFFER_SIZE];
};

struct net_tcp_options
{
	u16 mss;
	u8 mss_present;
	u8 window_scale;
	u8 window_scale_present;
	u8 sack_permitted;
	u8 sack_count;
	u32 sack_left[4];
	u32 sack_right[4];
};

struct net_tcp_state
{
	u32 next_isn;
	u32 now;
	u32 sent;
	u32 received;
	u32 established;
	u32 closed;
	u32 drops;
	u32 malformed;
	u32 checksum_errors;
	u32 option_errors;
	u32 ack_errors;
	u32 retransmissions;
	u32 fast_retransmits;
	u32 sack_blocks;
	u32 persist_probes;
	u32 persist_expired;
	struct net_tcp_listener listeners[NET_TCP_LISTENER_COUNT];
	struct net_tcp_connection connections[NET_TCP_CONNECTION_COUNT];
};

static int net_tcp_parse_options(const u8 *options, u16 length,
					 struct net_tcp_options *parsed)
{
	if (!parsed || (length && !options) || length > NET_TCP_OPTIONS_MAX_LENGTH)
		return -1;
	memset(parsed, 0, sizeof(*parsed));
	u16 offset = 0;
	while (offset < length) {
		u8 kind = options[offset++];
		if (kind == NET_TCP_OPTION_EOL) break;
		if (kind == NET_TCP_OPTION_NOP) continue;
		if (offset >= length) return -2;
		u8 option_length = options[offset++];
		if (option_length < 2 ||
		    option_length - 2 > length - offset) return -3;
		const u8 *value = options + offset;
		if (kind == NET_TCP_OPTION_MSS && option_length == 4) {
			u16 mss = ((u16)value[0] << 8) | value[1];
			if (mss) {
				parsed->mss = mss;
				parsed->mss_present = 1;
			}
		} else if (kind == NET_TCP_OPTION_WINDOW_SCALE &&
		           option_length == 3) {
			parsed->window_scale = value[0] > 14 ? 14 : value[0];
			parsed->window_scale_present = 1;
		} else if (kind == NET_TCP_OPTION_SACK_PERMITTED &&
		           option_length == 2) {
			parsed->sack_permitted = 1;
		} else if (kind == NET_TCP_OPTION_SACK && option_length >= 10 &&
		           ((option_length - 2) % 8) == 0) {
			u32 count = (option_length - 2) / 8;
			if (count > 4) count = 4;
			for (u32 index = 0; index < count; index++) {
				u32 left = 0;
				u32 right = 0;
				memcpy(&left, value + index * 8, 4);
				memcpy(&right, value + index * 8 + 4, 4);
				parsed->sack_left[index] = net_be32(left);
				parsed->sack_right[index] = net_be32(right);
			}
			parsed->sack_count = count;
		}
		offset += option_length - 2;
	}
	return 0;
}

static void net_tcp_init
(struct net_tcp_state *state)
{
	if (!state) return;
	memset(state, 0, sizeof(*state));
	state->next_isn = 0x1000;
}

static struct net_tcp_listener *net_tcp_find_listener(
	struct net_tcp_state *state, u16 port)
{
	if (!state || !port) return 0;
	for (u32 index = 0; index < NET_TCP_LISTENER_COUNT; index++)
		if (state->listeners[index].bound &&
		    state->listeners[index].port == port)
			return &state->listeners[index];
	return 0;
}

static int net_tcp_listen(struct net_tcp_state *state, u16 port)
{
	if (!state || !port) return -1;
	if (net_tcp_find_listener(state, port)) return 0;
	for (u32 index = 0; index < NET_TCP_LISTENER_COUNT; index++) {
		if (state->listeners[index].bound) continue;
		state->listeners[index].bound = 1;
		state->listeners[index].port = port;
		return 0;
	}
	return -2;
}

static int net_tcp_unlisten(struct net_tcp_state *state, u16 port)
{
	struct net_tcp_listener *listener = net_tcp_find_listener(state, port);
	if (!listener) return -1;
	memset(listener, 0, sizeof(*listener));
	return 0;
}

static struct net_tcp_connection *net_tcp_find_connection(
	struct net_tcp_state *state, const u8 *remote_ip,
	u16 local_port, u16 remote_port)
{
	if (!state || !remote_ip) return 0;
	for (u32 index = 0; index < NET_TCP_CONNECTION_COUNT; index++) {
		struct net_tcp_connection *connection = &state->connections[index];
		if (connection->used && connection->local_port == local_port &&
		    connection->remote_port == remote_port &&
		    net_ipv4_equal(connection->remote_ip, remote_ip))
			return connection;
	}
	return 0;
}

static struct net_tcp_connection *net_tcp_allocate_connection(
	struct net_tcp_state *state)
{
	if (!state) return 0;
	for (u32 index = 0; index < NET_TCP_CONNECTION_COUNT; index++) {
		if (state->connections[index].used) continue;
		memset(&state->connections[index], 0,
		       sizeof(state->connections[index]));
		state->connections[index].used = 1;
		state->connections[index].local_mss = NET_TCP_DEFAULT_MSS;
		state->connections[index].remote_mss = NET_TCP_DEFAULT_MSS;
		state->connections[index].local_window_scale =
			NET_TCP_DEFAULT_WINDOW_SCALE;
		state->connections[index].remote_window = NET_TCP_DEFAULT_MSS;
		state->connections[index].congestion_window =
			NET_TCP_DEFAULT_MSS * 2;
		state->connections[index].slow_start_threshold = 65535;
		state->connections[index].sack_permitted = 1;
		return &state->connections[index];
	}
	return 0;
}

static u32 net_tcp_segment_end(const struct net_tcp_tx_segment *segment)
{
	if (!segment) return 0;
	u32 end = segment->sequence + segment->length;
	if (segment->flags & (NET_TCP_FLAG_SYN | NET_TCP_FLAG_FIN)) end++;
	return end;
}

static int net_tcp_sequence_after(u32 left, u32 right)
{
	return left != right && (u32)(left - right) < 0x80000000u;
}

static u32 net_tcp_inflight(const struct net_tcp_connection *connection)
{
	if (!connection) return 0;
	u32 length = 0;
	for (u32 index = 0; index < NET_TCP_TX_SEGMENT_COUNT; index++) {
		const struct net_tcp_tx_segment *segment = &connection->tx[index];
		if (segment->used) length += net_tcp_segment_end(segment) -
			segment->sequence;
	}
	return length;
}

static u32 net_tcp_reorder_bytes(
	const struct net_tcp_connection *connection)
{
	if (!connection) return 0;
	u32 length = 0;
	for (u32 index = 0; index < NET_TCP_REORDER_SEGMENT_COUNT; index++) {
		const struct net_tcp_reorder_segment *segment =
			&connection->reorder[index];
		if (segment->used) length += segment->length;
	}
	return length;
}

static int net_tcp_queue_reorder(struct net_tcp_connection *connection,
					 u32 sequence, const void *payload, u16 length)
{
	if (!connection || !payload || !length ||
	    length > NET_TCP_PAYLOAD_MAX ||
	    connection->reorder_count >= NET_TCP_REORDER_SEGMENT_COUNT)
		return -1;
	for (u32 index = 0; index < NET_TCP_REORDER_SEGMENT_COUNT; index++) {
		struct net_tcp_reorder_segment *segment = &connection->reorder[index];
		if (segment->used && segment->sequence == sequence) return -3;
		if (segment->used) continue;
		segment->used = 1;
		segment->sequence = sequence;
		segment->length = length;
		memcpy(segment->payload, payload, length);
		connection->reorder_count++;
		return 0;
	}
	return -2;
}

static u32 net_tcp_drain_reorder(struct net_tcp_connection *connection,
					 struct net_tcp_state *state)
{
	if (!connection) return 0;
	u32 drained = 0;
	for (;;) {
		struct net_tcp_reorder_segment *match = 0;
		for (u32 index = 0; index < NET_TCP_REORDER_SEGMENT_COUNT; index++) {
			struct net_tcp_reorder_segment *segment =
				&connection->reorder[index];
			if (segment->used && segment->sequence ==
				connection->receive_sequence) {
				match = segment;
				break;
			}
		}
		if (!match || match->length > NET_TCP_RECV_BUFFER_SIZE -
			connection->receive_length) break;
		memcpy(connection->receive_buffer + connection->receive_length,
		       match->payload, match->length);
		connection->receive_length += match->length;
		connection->receive_sequence += match->length;
		drained += match->length;
		if (state) state->received++;
		memset(match, 0, sizeof(*match));
		if (connection->reorder_count) connection->reorder_count--;
	}
	return drained;
}

static void net_tcp_apply_sack(struct net_tcp_connection *connection,
					 const struct net_tcp_options *options)
{
	if (!connection || !connection->sack_permitted || !options ||
	    !options->sack_count) return;
	for (u32 index = 0; index < NET_TCP_TX_SEGMENT_COUNT; index++) {
		struct net_tcp_tx_segment *segment = &connection->tx[index];
		if (!segment->used) continue;
		u32 end = net_tcp_segment_end(segment);
		for (u32 block = 0; block < options->sack_count; block++) {
			if (options->sack_left[block] <= segment->sequence &&
			    options->sack_right[block] >= end &&
			    options->sack_left[block] < options->sack_right[block]) {
				segment->sacked = 1;
				break;
			}
		}
	}
}

static struct net_tcp_tx_segment *net_tcp_oldest_segment(
	struct net_tcp_connection *connection)
{
	if (!connection) return 0;
	struct net_tcp_tx_segment *oldest = 0;
	for (u32 index = 0; index < NET_TCP_TX_SEGMENT_COUNT; index++) {
		struct net_tcp_tx_segment *segment = &connection->tx[index];
		if (!segment->used || segment->sacked) continue;
		if (!oldest || (u32)(oldest->sequence - segment->sequence) <
			0x80000000u) oldest = segment;
	}
	return oldest;
}

static struct net_tcp_tx_segment *net_tcp_oldest_data_segment(
	struct net_tcp_connection *connection)
{
	if (!connection) return 0;
	struct net_tcp_tx_segment *oldest = 0;
	for (u32 index = 0; index < NET_TCP_TX_SEGMENT_COUNT; index++) {
		struct net_tcp_tx_segment *segment = &connection->tx[index];
		if (!segment->used || segment->sacked || !segment->length) continue;
		if (!oldest || (u32)(oldest->sequence - segment->sequence) <
			0x80000000u) oldest = segment;
	}
	return oldest;
}

static int net_tcp_queue_segment(struct net_tcp_connection *connection,
					 u16 flags, u32 sequence, u32 acknowledgement,
					 const void *payload, u16 length, u32 now)
{
	if (!connection || length > NET_TCP_PAYLOAD_MAX ||
	    (length && !payload)) return -1;
	for (u32 index = 0; index < NET_TCP_TX_SEGMENT_COUNT; index++) {
		struct net_tcp_tx_segment *segment = &connection->tx[index];
		if (segment->used) continue;
		memset(segment, 0, sizeof(*segment));
		segment->used = 1;
		segment->flags = flags;
		segment->length = length;
		segment->sequence = sequence;
		segment->acknowledgement = acknowledgement;
		segment->retransmit_at = now + NET_TCP_RETRANSMIT_TICKS;
		if (length) memcpy(segment->payload, payload, length);
		connection->tx_count++;
		return 0;
	}
	return -2;
}

static u16 net_tcp_acknowledge(struct net_tcp_connection *connection,
				       u32 acknowledgement, u32 *bytes)
{
	if (bytes) *bytes = 0;
	if (!connection) return 0;
	u16 acknowledged = 0;
	for (u32 index = 0; index < NET_TCP_TX_SEGMENT_COUNT; index++) {
		struct net_tcp_tx_segment *segment = &connection->tx[index];
		if (!segment->used || net_tcp_sequence_after(
				net_tcp_segment_end(segment), acknowledgement)) continue;
		acknowledged |= segment->flags;
		if (bytes) *bytes += net_tcp_segment_end(segment) - segment->sequence;
		memset(segment, 0, sizeof(*segment));
		if (connection->tx_count) connection->tx_count--;
	}
	return acknowledged;
}

static struct net_tcp_connection *net_tcp_first_established(
	struct net_tcp_state *state)
{
	if (!state) return 0;
	for (u32 index = 0; index < NET_TCP_CONNECTION_COUNT; index++)
		if (state->connections[index].used &&
		    state->connections[index].state == NET_TCP_STATE_ESTABLISHED)
			return &state->connections[index];
	return 0;
}

static struct net_tcp_connection *net_tcp_first_active(
	struct net_tcp_state *state)
{
	if (!state) return 0;
	for (u32 index = 0; index < NET_TCP_CONNECTION_COUNT; index++)
		if (state->connections[index].used)
			return &state->connections[index];
	return 0;
}

static struct net_tcp_connection *net_tcp_first_closable(
	struct net_tcp_state *state)
{
	if (!state) return 0;
	for (u32 index = 0; index < NET_TCP_CONNECTION_COUNT; index++)
		if (state->connections[index].used &&
		    (state->connections[index].state == NET_TCP_STATE_ESTABLISHED ||
		     state->connections[index].state == NET_TCP_STATE_CLOSE_WAIT))
			return &state->connections[index];
	return 0;
}

static u16 net_tcp_checksum(const u8 *source_ip, const u8 *destination_ip,
                            const void *data, u16 length)
{
	const u8 *bytes = (const u8 *)data;
	u32 sum = 0;
	for (u32 index = 0; index < 4; index += 2) {
		sum += ((u16)source_ip[index] << 8) | source_ip[index + 1];
		sum += ((u16)destination_ip[index] << 8) | destination_ip[index + 1];
	}
	sum += NET_IP_PROTOCOL_TCP;
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

static u16 net_tcp_build_options(
	const struct net_tcp_connection *connection, u16 flags, u8 *options)
{
	if (!connection || !options) return 0;
	u16 length = 0;
	if (flags & NET_TCP_FLAG_SYN) {
		options[length++] = NET_TCP_OPTION_MSS;
		options[length++] = 4;
		options[length++] = (u8)(connection->local_mss >> 8);
		options[length++] = (u8)connection->local_mss;
		if (!(flags & NET_TCP_FLAG_ACK) || connection->window_scale_enabled) {
			options[length++] = NET_TCP_OPTION_NOP;
			options[length++] = NET_TCP_OPTION_WINDOW_SCALE;
			options[length++] = 3;
			options[length++] = connection->local_window_scale;
		}
		if (connection->sack_permitted) {
			options[length++] = NET_TCP_OPTION_SACK_PERMITTED;
			options[length++] = 2;
		}
		options[length++] = NET_TCP_OPTION_EOL;
		while (length & 3) options[length++] = NET_TCP_OPTION_EOL;
		return length;
	}
	if (!connection->sack_permitted || !connection->reorder_count)
		return 0;
	options[length++] = NET_TCP_OPTION_NOP;
	options[length++] = NET_TCP_OPTION_NOP;
	u16 option_start = length;
	options[length++] = NET_TCP_OPTION_SACK;
	options[length++] = 0;
	u16 option_length = 2;
	u32 blocks = 0;
	for (u32 index = 0; index < NET_TCP_REORDER_SEGMENT_COUNT && blocks < 3;
	     index++) {
		const struct net_tcp_reorder_segment *segment =
			&connection->reorder[index];
		if (!segment->used) continue;
		u32 left = net_be32(segment->sequence);
		u32 right = net_be32(segment->sequence + segment->length);
		memcpy(options + length, &left, 4);
		memcpy(options + length + 4, &right, 4);
		length += 8;
		option_length += 8;
		blocks++;
	}
	if (!blocks) return 0;
	options[option_start + 1] = (u8)option_length;
	while (length & 3) options[length++] = NET_TCP_OPTION_EOL;
	return length;
}

static u32 net_tcp_receive_window(const struct net_tcp_connection *connection)
{
	if (!connection) return 0;
	u32 used = connection->receive_length + net_tcp_reorder_bytes(connection);
	return used >= NET_TCP_RECV_BUFFER_SIZE ? 0 :
		NET_TCP_RECV_BUFFER_SIZE - used;
}

static void net_tcp_update_remote_window(struct net_tcp_connection *connection,
					 u16 advertised)
{
	if (!connection) return;
	u32 shift = connection->window_scale_enabled ?
		connection->remote_window_scale : 0;
	u32 window = advertised;
	if (shift >= 31 || window > (0xFFFFFFFFu >> shift))
		window = 0xFFFFFFFFu;
	else window <<= shift;
	connection->remote_window = window;
}

static u32 net_tcp_send_limit(const struct net_tcp_connection *connection)
{
	if (!connection) return 0;
	return connection->remote_window < connection->congestion_window ?
		connection->remote_window : connection->congestion_window;
}

static void net_tcp_congestion_ack(struct net_tcp_connection *connection,
					 u32 bytes)
{
	if (!connection || !bytes) return;
	u32 mss = connection->local_mss ? connection->local_mss :
		NET_TCP_DEFAULT_MSS;
	if (connection->congestion_window < connection->slow_start_threshold) {
		connection->congestion_window += bytes;
	} else {
		u32 increase = (mss * mss) / connection->congestion_window;
		if (!increase) increase = 1;
		connection->congestion_window += increase;
	}
	if (connection->congestion_window < mss)
		connection->congestion_window = mss;
}

static void net_tcp_congestion_loss(struct net_tcp_connection *connection,
					 u32 fast_retransmit)
{
	if (!connection) return;
	u32 mss = connection->local_mss ? connection->local_mss :
		NET_TCP_DEFAULT_MSS;
	u32 in_flight = net_tcp_inflight(connection);
	u32 half = in_flight / 2;
	if (half < mss * 2) half = mss * 2;
	connection->slow_start_threshold = half;
	connection->congestion_window = fast_retransmit ? half : mss;
}

static int net_tcp_send_segment(struct net_tcp_state *state,
                                struct net_arp_state *arp,
                                struct net_tcp_connection *connection,
                                u16 flags, u32 sequence, u32 acknowledgement,
                                const void *payload, u16 payload_length,
                                net_send_frame_function send)
{
	if (!state || !arp || !connection || !connection->used || !send ||
	    payload_length > NET_TCP_PAYLOAD_MAX ||
	    (payload_length && !payload)) return -1;
	u8 frame[NET_ETHERNET_HEADER_LENGTH + NET_IPV4_HEADER_LENGTH +
	         NET_TCP_HEADER_LENGTH + NET_TCP_OPTIONS_MAX_LENGTH +
	         NET_TCP_PAYLOAD_MAX];
	memset(frame, 0, sizeof(frame));
	u8 options[NET_TCP_OPTIONS_MAX_LENGTH];
	u16 options_length = net_tcp_build_options(connection, flags, options);
	struct net_ethernet_header *ethernet =
		(struct net_ethernet_header *)frame;
	struct net_ipv4_header *ip =
		(struct net_ipv4_header *)(frame + NET_ETHERNET_HEADER_LENGTH);
	struct net_tcp_header *tcp =
		(struct net_tcp_header *)(frame + NET_ETHERNET_HEADER_LENGTH +
		                          NET_IPV4_HEADER_LENGTH);
	net_copy_mac(ethernet->destination, connection->remote_mac);
	net_copy_mac(ethernet->source, arp->local_mac);
	ethernet->type = net_be16(NET_ETHERTYPE_IPV4);
	u16 tcp_length = NET_TCP_HEADER_LENGTH + options_length + payload_length;
	u16 ip_length = NET_IPV4_HEADER_LENGTH + tcp_length;
	ip->version_ihl = (NET_IPV4_VERSION << 4) | 5;
	ip->total_length = net_be16(ip_length);
	ip->identification = 0;
	ip->flags_fragment = 0;
	ip->ttl = NET_IPV4_TTL;
	ip->protocol = NET_IP_PROTOCOL_TCP;
	ip->checksum = 0;
	memcpy(ip->source, arp->local_ip, 4);
	memcpy(ip->destination, connection->remote_ip, 4);
	ip->checksum = net_be16(net_checksum(ip, NET_IPV4_HEADER_LENGTH));
	tcp->source_port = net_be16(connection->local_port);
	tcp->destination_port = net_be16(connection->remote_port);
	tcp->sequence = net_be32(sequence);
	tcp->acknowledgement = net_be32(acknowledgement);
	tcp->data_offset_flags = net_be16(
		(((NET_TCP_HEADER_LENGTH + options_length) / 4) << 12) | flags);
	u32 receive_window = net_tcp_receive_window(connection);
	u32 window_shift = (flags & NET_TCP_FLAG_SYN) ? 0 :
		(connection->window_scale_enabled ? connection->local_window_scale : 0);
	u32 advertised_window = window_shift ? receive_window >> window_shift :
		receive_window;
	if (advertised_window > 65535) advertised_window = 65535;
	tcp->window = net_be16((u16)advertised_window);
	tcp->checksum = 0;
	if (options_length)
		memcpy((u8 *)tcp + NET_TCP_HEADER_LENGTH, options, options_length);
	if (payload_length)
		memcpy((u8 *)tcp + NET_TCP_HEADER_LENGTH + options_length,
		       payload, payload_length);
	tcp->checksum = net_be16(net_tcp_checksum(
		ip->source, ip->destination, tcp, tcp_length));
	u32 frame_length = NET_ETHERNET_HEADER_LENGTH + ip_length;
	if (frame_length < NET_ETHERNET_MIN_FRAME_LENGTH)
		frame_length = NET_ETHERNET_MIN_FRAME_LENGTH;
	int result = send(frame, frame_length);
	if (!result) state->sent++;
	return result;
}

static int net_tcp_connect(struct net_tcp_state *state,
                           struct net_arp_state *arp,
                           const u8 *remote_ip, u16 remote_port,
                           net_send_frame_function send)
{
	if (!state || !arp || !remote_ip || !remote_port || !send) return -1;
	u8 remote_mac[NET_ETHERNET_ADDRESS_LENGTH];
	int lookup = net_arp_lookup(arp, remote_ip, remote_mac);
	if (lookup == 1) {
		int result = net_arp_request(arp, remote_ip, send);
		return result ? result : 1;
	}
	if (lookup) return lookup;
	struct net_tcp_connection *connection =
		net_tcp_allocate_connection(state);
	if (!connection) return -2;
	connection->local_port = 40000;
	while (net_tcp_find_connection(state, remote_ip, connection->local_port,
	                               remote_port))
		connection->local_port++;
	connection->remote_port = remote_port;
	connection->remote_window = NET_TCP_RECV_BUFFER_SIZE;
	memcpy(connection->remote_ip, remote_ip, 4);
	net_copy_mac(connection->remote_mac, remote_mac);
	connection->send_sequence = state->next_isn++;
	int result = net_tcp_send_segment(
		state, arp, connection, NET_TCP_FLAG_SYN,
		connection->send_sequence, 0, 0, 0, send);
	if (result) {
		memset(connection, 0, sizeof(*connection));
		return result;
	}
	if (net_tcp_queue_segment(connection, NET_TCP_FLAG_SYN,
				connection->send_sequence, 0, 0, 0,
				state->now)) {
		memset(connection, 0, sizeof(*connection));
		return -3;
	}
	connection->send_sequence++;
	connection->state = NET_TCP_STATE_SYN_SENT;
	connection->last_activity = state->now;
	return 0;
}

static int net_tcp_send_data(struct net_tcp_state *state,
                             struct net_arp_state *arp,
                             const void *payload, u16 length,
                             net_send_frame_function send)
{
	struct net_tcp_connection *connection =
		net_tcp_first_established(state);
	if (!connection || !payload || !length || length > NET_TCP_PAYLOAD_MAX)
		return -1;
	u32 mss = connection->remote_mss ? connection->remote_mss :
		NET_TCP_DEFAULT_MSS;
	if (mss > NET_TCP_PAYLOAD_MAX) mss = NET_TCP_PAYLOAD_MAX;
	u32 required_segments = (length + mss - 1) / mss;
	if (required_segments > NET_TCP_TX_SEGMENT_COUNT - connection->tx_count)
		return -1;
	u32 inflight = net_tcp_inflight(connection);
	u32 send_limit = net_tcp_send_limit(connection);
	if (inflight > send_limit || length > send_limit - inflight)
		return -1;
	const u8 *bytes = (const u8 *)payload;
	u32 offset = 0;
	while (offset < length) {
		u16 chunk = length - offset;
		if (chunk > mss) chunk = mss;
		u32 sequence = connection->send_sequence;
		int result = net_tcp_send_segment(
			state, arp, connection, NET_TCP_FLAG_ACK | NET_TCP_FLAG_PSH,
			sequence, connection->receive_sequence, bytes + offset, chunk,
			send);
		if (result) return result;
		if (net_tcp_queue_segment(connection,
				NET_TCP_FLAG_ACK | NET_TCP_FLAG_PSH, sequence,
				connection->receive_sequence, bytes + offset, chunk,
				state->now)) return -2;
		connection->send_sequence += chunk;
		offset += chunk;
	}
	connection->last_activity = state->now;
	return 0;
}

static int net_tcp_receive_data(struct net_tcp_state *state,
                                void *buffer, u16 capacity, u16 *length)
{
	if (length) *length = 0;
	if (!state || !buffer || !capacity || !length) return -1;
	struct net_tcp_connection *connection =
		net_tcp_first_established(state);
	if (!connection) return -2;
	if (!connection->receive_length) return 1;
	u16 copy_length = connection->receive_length;
	if (copy_length > capacity) copy_length = capacity;
	memcpy(buffer, connection->receive_buffer, copy_length);
	for (u32 index = copy_length; index < connection->receive_length; index++)
		connection->receive_buffer[index - copy_length] =
			connection->receive_buffer[index];
	connection->receive_length -= copy_length;
	*length = copy_length;
	return 0;
}

static int net_tcp_close(struct net_tcp_state *state,
                         struct net_arp_state *arp,
                         net_send_frame_function send)
{
	struct net_tcp_connection *connection =
		net_tcp_first_closable(state);
	if (!connection || connection->tx_count >= NET_TCP_TX_SEGMENT_COUNT ||
	    net_tcp_inflight(connection) >= net_tcp_send_limit(connection))
		return -1;
	u32 sequence = connection->send_sequence;
	u32 next_state = connection->state == NET_TCP_STATE_CLOSE_WAIT ?
		NET_TCP_STATE_LAST_ACK : NET_TCP_STATE_FIN_WAIT_1;
	int result = net_tcp_send_segment(
		state, arp, connection, NET_TCP_FLAG_FIN | NET_TCP_FLAG_ACK,
		sequence, connection->receive_sequence, 0, 0, send);
	if (!result) {
		if (net_tcp_queue_segment(connection, NET_TCP_FLAG_FIN | NET_TCP_FLAG_ACK,
				sequence, connection->receive_sequence, 0, 0,
				state->now)) return -2;
		connection->send_sequence++;
		connection->state = next_state;
		connection->last_activity = state->now;
	}
	return result;
}

static void net_tcp_enter_time_wait(
	struct net_tcp_connection *connection, u32 now)
{
	if (!connection) return;
	connection->state = NET_TCP_STATE_TIME_WAIT;
	connection->time_wait_at = now + NET_TCP_TIME_WAIT_TICKS;
	connection->last_activity = now;
}

static int net_tcp_tick(struct net_tcp_state *state,
                         struct net_arp_state *arp, u32 now,
                         net_send_frame_function send)
{
	if (!state || !arp || !send) return -1;
	state->now = now;
	int events = 0;
	for (u32 index = 0; index < NET_TCP_CONNECTION_COUNT; index++) {
		struct net_tcp_connection *connection = &state->connections[index];
		if (!connection->used) continue;
		if (connection->state == NET_TCP_STATE_TIME_WAIT) {
			if ((u32)(now - connection->time_wait_at) < 0x80000000u) {
				memset(connection, 0, sizeof(*connection));
				state->closed++;
				events++;
			}
			continue;
		}
		if (connection->state == NET_TCP_STATE_ESTABLISHED &&
		    connection->remote_window == 0 && connection->tx_count) {
			u32 elapsed = now - connection->last_activity;
			if (elapsed < 0x80000000u &&
			    elapsed >= NET_TCP_IDLE_TIMEOUT_TICKS) {
				memset(connection, 0, sizeof(*connection));
				state->closed++;
				events++;
				continue;
			}
			if (connection->persist_count >= NET_TCP_MAX_RETRANSMITS) {
				state->persist_expired++;
				memset(connection, 0, sizeof(*connection));
				state->closed++;
				events++;
				continue;
			}
			if ((u32)(now - connection->persist_at) < 0x80000000u) {
				struct net_tcp_tx_segment *segment =
					net_tcp_oldest_data_segment(connection);
				if (segment) {
					state->persist_probes++;
					connection->persist_probes++;
					int result = net_tcp_send_segment(
						state, arp, connection, segment->flags,
						segment->sequence, connection->receive_sequence,
						segment->payload, 1, send);
					connection->persist_at =
						now + NET_TCP_PERSIST_TICKS;
					connection->persist_count++;
					if (!result) events++;
				}
			}
			continue;
		}
		u32 failed = 0;
		u32 congestion_loss_applied = 0;
		for (u32 segment_index = 0;
		     segment_index < NET_TCP_TX_SEGMENT_COUNT; segment_index++) {
			struct net_tcp_tx_segment *segment =
				&connection->tx[segment_index];
			if (!segment->used || segment->sacked ||
			    (u32)(now - segment->retransmit_at) >= 0x80000000u)
				continue;
			if (segment->retransmit_count >= NET_TCP_MAX_RETRANSMITS) {
				failed = 1;
				break;
			}
			if (!congestion_loss_applied) {
				net_tcp_congestion_loss(connection, 0);
				congestion_loss_applied = 1;
			}
			state->retransmissions++;
			connection->retransmissions++;
			int result = net_tcp_send_segment(
				state, arp, connection, segment->flags,
				segment->sequence, connection->receive_sequence,
				segment->length ? segment->payload : 0,
				segment->length, send);
			segment->retransmit_at = now + NET_TCP_RETRANSMIT_TICKS;
			segment->retransmit_count++;
			if (!result) {
				connection->last_activity = now;
				events++;
			}
		}
		if (failed) {
			memset(connection, 0, sizeof(*connection));
			state->closed++;
			events++;
			continue;
		}
		if (connection->state != NET_TCP_STATE_CLOSED &&
		    (u32)(now - connection->last_activity) < 0x80000000u &&
		    now - connection->last_activity >= NET_TCP_IDLE_TIMEOUT_TICKS) {
			memset(connection, 0, sizeof(*connection));
			state->closed++;
			events++;
		}
	}
	return events;
}

static int net_tcp_handle_frame(struct net_tcp_state *state,
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
	u32 ip_header_length = (ip->version_ihl & 0x0F) * 4;
	u16 total_length = net_be16(ip->total_length);
	if (version != NET_IPV4_VERSION ||
	    ip_header_length < NET_IPV4_HEADER_LENGTH ||
	    total_length < ip_header_length ||
	    total_length > length - NET_ETHERNET_HEADER_LENGTH ||
	    (net_be16(ip->flags_fragment) & 0x3FFF)) {
		state->malformed++;
		return -2;
	}
	if (net_checksum(ip, ip_header_length) != 0) {
		state->malformed++;
		state->checksum_errors++;
		return -2;
	}
	if (ip->protocol != NET_IP_PROTOCOL_TCP ||
	    !net_ipv4_equal(ip->destination, arp->local_ip)) return 0;
	net_arp_cache_update(arp, ip->source, ethernet->source);
	u16 tcp_length = total_length - ip_header_length;
	if (tcp_length < NET_TCP_HEADER_LENGTH) {
		state->drops++;
		state->malformed++;
		return -3;
	}
	struct net_tcp_header *tcp =
		(struct net_tcp_header *)(frame + NET_ETHERNET_HEADER_LENGTH +
		                          ip_header_length);
	u16 flags_word = net_be16(tcp->data_offset_flags);
	u32 tcp_header_length = (flags_word >> 12) * 4;
	u16 flags = flags_word & 0x01FF;
	if (tcp_header_length < NET_TCP_HEADER_LENGTH ||
	    tcp_header_length > tcp_length) {
		state->drops++;
		state->malformed++;
		return -3;
	}
	if (net_tcp_checksum(ip->source, ip->destination, tcp, tcp_length)) {
		state->drops++;
		state->malformed++;
		state->checksum_errors++;
		return -3;
	}
	struct net_tcp_options parsed_options;
	if (net_tcp_parse_options((u8 *)tcp + NET_TCP_HEADER_LENGTH,
			tcp_header_length - NET_TCP_HEADER_LENGTH, &parsed_options)) {
		state->drops++;
		state->malformed++;
		state->option_errors++;
		return -4;
	}
	u16 source_port = net_be16(tcp->source_port);
	u16 destination_port = net_be16(tcp->destination_port);
	u32 sequence = net_be32(tcp->sequence);
	u32 acknowledgement = net_be32(tcp->acknowledgement);
	u16 payload_length = tcp_length - tcp_header_length;
	u8 *payload = (u8 *)tcp + tcp_header_length;
	if ((flags & NET_TCP_FLAG_SYN) &&
	    (flags & (NET_TCP_FLAG_FIN | NET_TCP_FLAG_RST))) {
		state->drops++;
		state->malformed++;
		return -11;
	}
	struct net_tcp_connection *connection = net_tcp_find_connection(
		state, ip->source, destination_port, source_port);
	if (!connection && (flags & NET_TCP_FLAG_SYN)) {
		if (!net_tcp_find_listener(state, destination_port)) return 0;
		connection = net_tcp_allocate_connection(state);
		if (!connection) return -4;
		connection->local_port = destination_port;
		connection->remote_port = source_port;
		if (parsed_options.mss_present && parsed_options.mss <
			connection->remote_mss)
			connection->remote_mss = parsed_options.mss;
		connection->window_scale_enabled =
			parsed_options.window_scale_present;
		connection->remote_window_scale = parsed_options.window_scale;
		connection->sack_permitted = parsed_options.sack_permitted;
		net_tcp_update_remote_window(connection, net_be16(tcp->window));
		memcpy(connection->remote_ip, ip->source, 4);
		net_copy_mac(connection->remote_mac, ethernet->source);
		connection->receive_sequence = sequence + 1;
		connection->send_sequence = state->next_isn++;
		int result = net_tcp_send_segment(
			state, arp, connection, NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK,
			connection->send_sequence, connection->receive_sequence,
			0, 0, send);
		if (result) {
			memset(connection, 0, sizeof(*connection));
			return -5;
		}
		if (net_tcp_queue_segment(connection, NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK,
				connection->send_sequence, connection->receive_sequence,
				0, 0, state->now)) {
			memset(connection, 0, sizeof(*connection));
			return -5;
		}
		connection->send_sequence++;
		connection->state = NET_TCP_STATE_SYN_RECEIVED;
		connection->last_activity = state->now;
		return NET_NETWORK_POLL_TCP_ESTABLISHED;
	}
	if (!connection) return 0;
	if (connection->state == NET_TCP_STATE_SYN_SENT &&
	    (flags & NET_TCP_FLAG_SYN)) {
		if (parsed_options.mss_present && parsed_options.mss <
			connection->remote_mss)
			connection->remote_mss = parsed_options.mss;
		connection->window_scale_enabled =
			parsed_options.window_scale_present;
		if (!connection->window_scale_enabled)
			connection->local_window_scale = 0;
		connection->remote_window_scale = parsed_options.window_scale;
		connection->sack_permitted = connection->sack_permitted &&
			parsed_options.sack_permitted;
	}
	net_tcp_update_remote_window(connection, net_be16(tcp->window));
	if (connection->remote_window == 0 && connection->tx_count &&
	    connection->state == NET_TCP_STATE_ESTABLISHED) {
		if (!connection->persist_count)
			connection->persist_at = state->now + NET_TCP_PERSIST_TICKS;
	} else if (connection->remote_window) {
		connection->persist_at = 0;
		connection->persist_count = 0;
	}
	if (flags & NET_TCP_FLAG_RST) {
		memset(connection, 0, sizeof(*connection));
		state->closed++;
		return NET_NETWORK_POLL_TCP_CLOSED;
	}
	u16 acknowledged_flags = 0;
	u32 acknowledged_bytes = 0;
	if (flags & NET_TCP_FLAG_ACK) {
		if (net_tcp_sequence_after(acknowledgement,
				connection->send_sequence)) {
			state->drops++;
			state->malformed++;
			state->ack_errors++;
			return -9;
		}
		acknowledged_flags = net_tcp_acknowledge(connection,
		                                         acknowledgement,
		                                         &acknowledged_bytes);
		net_tcp_apply_sack(connection, &parsed_options);
		if (parsed_options.sack_count) {
			state->sack_blocks += parsed_options.sack_count;
			connection->sack_blocks += parsed_options.sack_count;
		}
		if (acknowledged_bytes)
			net_tcp_congestion_ack(connection, acknowledged_bytes);
		if (net_tcp_sequence_after(acknowledgement,
				connection->last_acknowledgement)) {
			connection->last_acknowledgement = acknowledgement;
			connection->duplicate_ack_count = 0;
		} else if (!acknowledged_flags &&
		           acknowledgement == connection->last_acknowledgement &&
		           connection->state == NET_TCP_STATE_ESTABLISHED) {
			connection->duplicate_ack_count++;
		}
		if (connection->duplicate_ack_count >= 3 &&
		    connection->state == NET_TCP_STATE_ESTABLISHED) {
			net_tcp_congestion_loss(connection, 1);
			struct net_tcp_tx_segment *segment =
				net_tcp_oldest_segment(connection);
			if (segment && segment->retransmit_count < NET_TCP_MAX_RETRANSMITS) {
				state->fast_retransmits++;
				connection->fast_retransmits++;
				int result = net_tcp_send_segment(
					state, arp, connection, segment->flags,
					segment->sequence, connection->receive_sequence,
					segment->length ? segment->payload : 0,
					segment->length, send);
				segment->retransmit_at =
					state->now + NET_TCP_RETRANSMIT_TICKS;
				segment->retransmit_count++;
				if (result) return -12;
			}
			connection->duplicate_ack_count = 0;
		}
		if (acknowledged_flags) connection->last_activity = state->now;
		if (acknowledged_flags & NET_TCP_FLAG_FIN) {
			if (connection->state == NET_TCP_STATE_FIN_WAIT_1)
				connection->state = NET_TCP_STATE_FIN_WAIT_2;
			else if (connection->state == NET_TCP_STATE_CLOSING)
				net_tcp_enter_time_wait(connection, state->now);
			else if (connection->state == NET_TCP_STATE_LAST_ACK) {
				memset(connection, 0, sizeof(*connection));
				state->closed++;
				return NET_NETWORK_POLL_TCP_CLOSED;
			}
		}
	}
	if (connection->state == NET_TCP_STATE_SYN_SENT &&
	    (flags & (NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK)) ==
			(NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK) &&
	    acknowledgement == connection->send_sequence) {
		connection->receive_sequence = sequence + 1;
		connection->state = NET_TCP_STATE_ESTABLISHED;
		int result = net_tcp_send_segment(
			state, arp, connection, NET_TCP_FLAG_ACK,
			connection->send_sequence, connection->receive_sequence,
			0, 0, send);
		if (result) return -6;
		state->established++;
		return NET_NETWORK_POLL_TCP_ESTABLISHED;
	}
	if (connection->state == NET_TCP_STATE_SYN_RECEIVED &&
	    (flags & NET_TCP_FLAG_ACK) &&
	    sequence == connection->receive_sequence &&
	    acknowledgement == connection->send_sequence) {
		connection->state = NET_TCP_STATE_ESTABLISHED;
		state->established++;
		return NET_NETWORK_POLL_TCP_ESTABLISHED;
	}
	if (connection->state == NET_TCP_STATE_TIME_WAIT) {
		if (flags & NET_TCP_FLAG_FIN)
			return net_tcp_send_segment(state, arp, connection,
				NET_TCP_FLAG_ACK, connection->send_sequence,
				connection->receive_sequence, 0, 0, send);
		return 0;
	}
	if (connection->state != NET_TCP_STATE_ESTABLISHED &&
	    connection->state != NET_TCP_STATE_FIN_WAIT_1 &&
	    connection->state != NET_TCP_STATE_FIN_WAIT_2 &&
	    connection->state != NET_TCP_STATE_CLOSING &&
	    connection->state != NET_TCP_STATE_CLOSE_WAIT &&
	    connection->state != NET_TCP_STATE_LAST_ACK) return 0;
	if (payload_length && connection->state != NET_TCP_STATE_ESTABLISHED) {
		state->drops++;
		state->malformed++;
		return -10;
	}
	if (payload_length && !(flags & NET_TCP_FLAG_ACK)) {
		state->drops++;
		state->malformed++;
		return -10;
	}
	if (flags & NET_TCP_FLAG_ACK) connection->last_activity = state->now;
	if (payload_length) {
		u32 receive_used = connection->receive_length +
			net_tcp_reorder_bytes(connection);
		if (sequence == connection->receive_sequence &&
		    payload_length <= NET_TCP_RECV_BUFFER_SIZE -
		                     receive_used) {
			memcpy(connection->receive_buffer + connection->receive_length,
			       payload, payload_length);
			connection->receive_length += payload_length;
			connection->receive_sequence += payload_length;
			state->received++;
			net_tcp_drain_reorder(connection, state);
			if (net_tcp_send_segment(state, arp, connection, NET_TCP_FLAG_ACK,
					connection->send_sequence, connection->receive_sequence,
					0, 0, send)) return -7;
			return NET_NETWORK_POLL_TCP_DATA;
		}
		if ((u32)(sequence - connection->receive_sequence) < 0x80000000u &&
		    payload_length <= NET_TCP_RECV_BUFFER_SIZE - receive_used &&
		    !net_tcp_queue_reorder(connection, sequence, payload,
					payload_length)) {
			if (net_tcp_send_segment(state, arp, connection, NET_TCP_FLAG_ACK,
					connection->send_sequence, connection->receive_sequence,
					0, 0, send)) return -7;
			return 0;
		}
		state->drops++;
		if (net_tcp_send_segment(state, arp, connection, NET_TCP_FLAG_ACK,
				connection->send_sequence, connection->receive_sequence,
				0, 0, send)) return -7;
		return 0;
	}
	if (flags & NET_TCP_FLAG_FIN) {
		if (sequence != connection->receive_sequence) {
			if (net_tcp_send_segment(state, arp, connection, NET_TCP_FLAG_ACK,
					connection->send_sequence, connection->receive_sequence,
					0, 0, send)) return -11;
			return 0;
		}
		connection->receive_sequence++;
		if (connection->state == NET_TCP_STATE_ESTABLISHED)
			connection->state = NET_TCP_STATE_CLOSE_WAIT;
		else if (connection->state == NET_TCP_STATE_FIN_WAIT_1)
			connection->state = NET_TCP_STATE_CLOSING;
		else if (connection->state == NET_TCP_STATE_FIN_WAIT_2 ||
		         connection->state == NET_TCP_STATE_CLOSING)
			net_tcp_enter_time_wait(connection, state->now);
		if (net_tcp_send_segment(state, arp, connection, NET_TCP_FLAG_ACK,
				connection->send_sequence, connection->receive_sequence,
				0, 0, send)) return -8;
		return NET_NETWORK_POLL_TCP_FIN;
	}
	return 0;
}
