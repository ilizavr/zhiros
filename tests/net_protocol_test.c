#include <stdio.h>
#include <string.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

#include "net/ethernet.h"
#include "net/arp.h"
#include "net/ipv4.h"
#include "net/tcp.h"
#include "net/udp.h"
#include "net/dhcp.h"

static u8 sent_frame[NET_PACKET_BUFFER_SIZE];
static u32 sent_length;
static u32 sent_count;

static int test_send(const void *frame, u32 length)
{
	if (!frame || length > sizeof(sent_frame)) return -1;
	memcpy(sent_frame, frame, length);
	sent_length = length;
	sent_count++;
	return 0;
}

static void clear_sent()
{
	memset(sent_frame, 0, sizeof(sent_frame));
	sent_length = 0;
	sent_count = 0;
}

static int check(int condition, const char *name)
{
	if (condition) return 0;
	printf("FAIL: %s\n", name);
	return 1;
}

static void build_arp_request(u8 *frame, const u8 *peer_mac,
                              const u8 *peer_ip, const u8 *local_ip)
{
	static const u8 broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	static const u8 empty_mac[6] = {0, 0, 0, 0, 0, 0};
	memset(frame, 0, NET_ETHERNET_MIN_FRAME_LENGTH);
	struct net_arp_packet *packet = (struct net_arp_packet *)frame;
	net_copy_mac(packet->ethernet.destination, broadcast);
	net_copy_mac(packet->ethernet.source, peer_mac);
	packet->ethernet.type = net_be16(NET_ETHERTYPE_ARP);
	packet->hardware_type = net_be16(NET_ARP_HARDWARE_ETHERNET);
	packet->protocol_type = net_be16(NET_ETHERTYPE_IPV4);
	packet->hardware_length = 6;
	packet->protocol_length = 4;
	packet->operation = net_be16(NET_ARP_OPERATION_REQUEST);
	net_copy_mac(packet->sender_hardware, peer_mac);
	memcpy(packet->sender_protocol, peer_ip, 4);
	net_copy_mac(packet->target_hardware, empty_mac);
	memcpy(packet->target_protocol, local_ip, 4);
}

static u32 build_icmp_request(u8 *frame, const u8 *peer_mac,
                              const u8 *peer_ip, const u8 *local_mac,
                              const u8 *local_ip, u16 identifier)
{
	static const u8 payload[] = {1, 2, 3, 4};
	memset(frame, 0, NET_PACKET_BUFFER_SIZE);
	struct net_ethernet_header *ethernet =
		(struct net_ethernet_header *)frame;
	struct net_ipv4_header *ip =
		(struct net_ipv4_header *)(frame + NET_ETHERNET_HEADER_LENGTH);
	struct net_icmp_header *icmp =
		(struct net_icmp_header *)(frame + NET_ETHERNET_HEADER_LENGTH +
		                           NET_IPV4_HEADER_LENGTH);
	net_copy_mac(ethernet->destination, local_mac);
	net_copy_mac(ethernet->source, peer_mac);
	ethernet->type = net_be16(NET_ETHERTYPE_IPV4);
	ip->version_ihl = (NET_IPV4_VERSION << 4) | 5;
	ip->total_length = net_be16(NET_IPV4_HEADER_LENGTH +
	                            NET_ICMP_HEADER_LENGTH + sizeof(payload));
	ip->ttl = NET_IPV4_TTL;
	ip->protocol = NET_IP_PROTOCOL_ICMP;
	memcpy(ip->source, peer_ip, 4);
	memcpy(ip->destination, local_ip, 4);
	ip->checksum = net_be16(net_checksum(ip, NET_IPV4_HEADER_LENGTH));
	icmp->type = NET_ICMP_ECHO_REQUEST;
	icmp->identifier = net_be16(identifier);
	memcpy((u8 *)icmp + NET_ICMP_HEADER_LENGTH, payload, sizeof(payload));
	icmp->checksum = net_be16(net_checksum(
		icmp, NET_ICMP_HEADER_LENGTH + sizeof(payload)));
	return NET_ETHERNET_HEADER_LENGTH + net_be16(ip->total_length);
}

static u32 build_udp_request(u8 *frame, const u8 *peer_mac,
                             const u8 *peer_ip, const u8 *local_mac,
                             const u8 *local_ip, u16 destination_port)
{
	static const u8 payload[] = {'e', 'c', 'h', 'o'};
	u16 udp_length = NET_UDP_HEADER_LENGTH + sizeof(payload);
	memset(frame, 0, NET_PACKET_BUFFER_SIZE);
	struct net_ethernet_header *ethernet =
		(struct net_ethernet_header *)frame;
	struct net_ipv4_header *ip =
		(struct net_ipv4_header *)(frame + NET_ETHERNET_HEADER_LENGTH);
	struct net_udp_header *udp =
		(struct net_udp_header *)(frame + NET_ETHERNET_HEADER_LENGTH +
		                          NET_IPV4_HEADER_LENGTH);
	net_copy_mac(ethernet->destination, local_mac);
	net_copy_mac(ethernet->source, peer_mac);
	ethernet->type = net_be16(NET_ETHERTYPE_IPV4);
	ip->version_ihl = (NET_IPV4_VERSION << 4) | 5;
	ip->total_length = net_be16(NET_IPV4_HEADER_LENGTH + udp_length);
	ip->ttl = NET_IPV4_TTL;
	ip->protocol = NET_IP_PROTOCOL_UDP;
	memcpy(ip->source, peer_ip, 4);
	memcpy(ip->destination, local_ip, 4);
	ip->checksum = net_be16(net_checksum(ip, NET_IPV4_HEADER_LENGTH));
	udp->source_port = net_be16(9000);
	udp->destination_port = net_be16(destination_port);
	udp->length = net_be16(udp_length);
	memcpy((u8 *)udp + NET_UDP_HEADER_LENGTH, payload, sizeof(payload));
	udp->checksum = net_be16(net_udp_checksum(
		ip->source, ip->destination, udp, udp_length));
	return NET_ETHERNET_HEADER_LENGTH + net_be16(ip->total_length);
}

static u32 build_tcp_request(u8 *frame, const u8 *peer_mac,
                              const u8 *peer_ip, const u8 *local_mac,
                              const u8 *local_ip, u16 source_port,
                              u16 destination_port, u32 sequence,
                              u32 acknowledgement, u16 flags,
                              const u8 *options, u16 options_length,
                              const void *payload, u16 payload_length)
{
	u16 tcp_length = NET_TCP_HEADER_LENGTH + options_length + payload_length;
	u16 ip_length = NET_IPV4_HEADER_LENGTH + tcp_length;
	memset(frame, 0, NET_PACKET_BUFFER_SIZE);
	struct net_ethernet_header *ethernet =
		(struct net_ethernet_header *)frame;
	struct net_ipv4_header *ip =
		(struct net_ipv4_header *)(frame + NET_ETHERNET_HEADER_LENGTH);
	struct net_tcp_header *tcp =
		(struct net_tcp_header *)(frame + NET_ETHERNET_HEADER_LENGTH +
		                          NET_IPV4_HEADER_LENGTH);
	net_copy_mac(ethernet->destination, local_mac);
	net_copy_mac(ethernet->source, peer_mac);
	ethernet->type = net_be16(NET_ETHERTYPE_IPV4);
	ip->version_ihl = (NET_IPV4_VERSION << 4) | 5;
	ip->total_length = net_be16(ip_length);
	ip->ttl = NET_IPV4_TTL;
	ip->protocol = NET_IP_PROTOCOL_TCP;
	memcpy(ip->source, peer_ip, 4);
	memcpy(ip->destination, local_ip, 4);
	ip->checksum = net_be16(net_checksum(ip, NET_IPV4_HEADER_LENGTH));
	tcp->source_port = net_be16(source_port);
	tcp->destination_port = net_be16(destination_port);
	tcp->sequence = net_be32(sequence);
	tcp->acknowledgement = net_be32(acknowledgement);
	tcp->data_offset_flags = net_be16(
		(((NET_TCP_HEADER_LENGTH + options_length) / 4) << 12) | flags);
	tcp->window = net_be16(512);
	if (options_length)
		memcpy((u8 *)tcp + NET_TCP_HEADER_LENGTH, options, options_length);
	if (payload_length)
		memcpy((u8 *)tcp + NET_TCP_HEADER_LENGTH + options_length,
		       payload, payload_length);
	tcp->checksum = net_be16(net_tcp_checksum(
		ip->source, ip->destination, tcp, tcp_length));
	return NET_ETHERNET_HEADER_LENGTH + ip_length;
}

static int tcp_has_option(const u8 *frame, u8 wanted)
{
	const struct net_tcp_header *tcp =
		(const struct net_tcp_header *)(frame + NET_ETHERNET_HEADER_LENGTH +
		                                NET_IPV4_HEADER_LENGTH);
	u16 word = net_be16(tcp->data_offset_flags);
	u16 length = ((word >> 12) * 4) - NET_TCP_HEADER_LENGTH;
	const u8 *options = (const u8 *)tcp + NET_TCP_HEADER_LENGTH;
	for (u16 offset = 0; offset < length;) {
		u8 kind = options[offset++];
		if (kind == NET_TCP_OPTION_EOL) break;
		if (kind == NET_TCP_OPTION_NOP) continue;
		if (offset >= length || options[offset] < 2 ||
		    options[offset] - 2 > length - offset) return 0;
		u8 option_length = options[offset++];
		if (kind == wanted) return 1;
		offset += option_length - 2;
	}
	return 0;
}

static void set_tcp_window(u8 *frame, u16 window)
{
	struct net_ipv4_header *ip =
		(struct net_ipv4_header *)(frame + NET_ETHERNET_HEADER_LENGTH);
	struct net_tcp_header *tcp =
		(struct net_tcp_header *)(frame + NET_ETHERNET_HEADER_LENGTH +
		                          NET_IPV4_HEADER_LENGTH);
	u16 tcp_length = net_be16(ip->total_length) - NET_IPV4_HEADER_LENGTH;
	tcp->window = net_be16(window);
	tcp->checksum = 0;
	tcp->checksum = net_be16(net_tcp_checksum(
		ip->source, ip->destination, tcp, tcp_length));
}

static void add_dhcp_option(u8 *options, u32 *offset, u8 code,
                            u8 length, const void *data)
{
	options[(*offset)++] = code;
	options[(*offset)++] = length;
	memcpy(options + *offset, data, length);
	*offset += length;
}

static void build_dhcp_reply(struct net_udp_datagram *datagram,
                             const struct net_dhcp_state *state, u8 message,
                             const u8 *offered_ip, const u8 *server_ip)
{
	memset(datagram, 0, sizeof(*datagram));
	struct net_dhcp_header *packet =
		(struct net_dhcp_header *)datagram->payload;
	packet->operation = 2;
	packet->hardware_type = 1;
	packet->hardware_length = 6;
	packet->transaction_id = net_be32(state->transaction_id);
	memcpy(packet->your_ip, offered_ip, 4);
	net_copy_mac(packet->client_hardware, state->client_mac);
	u8 *options = datagram->payload + NET_DHCP_FIXED_LENGTH;
	u32 offset = 0;
	u32 cookie = net_be32(NET_DHCP_MAGIC_COOKIE);
	memcpy(options + offset, &cookie, 4);
	offset += 4;
	add_dhcp_option(options, &offset, NET_DHCP_OPTION_MESSAGE_TYPE,
	                1, &message);
	add_dhcp_option(options, &offset, NET_DHCP_OPTION_SERVER_IDENTIFIER,
	                4, server_ip);
	u8 mask[4] = {255, 255, 255, 0};
	u8 gateway[4] = {10, 0, 2, 1};
	u32 lease = net_be32(3600);
	add_dhcp_option(options, &offset, NET_DHCP_OPTION_SUBNET_MASK,
	                4, mask);
	add_dhcp_option(options, &offset, NET_DHCP_OPTION_ROUTER,
	                4, gateway);
	add_dhcp_option(options, &offset, NET_DHCP_OPTION_LEASE_TIME,
	                4, &lease);
	options[offset++] = 255;
	datagram->length = NET_DHCP_FIXED_LENGTH + offset;
}

int main()
{
	static const u8 local_mac[6] = {0x52, 0x54, 0, 0x12, 0x34, 0x56};
	static const u8 peer_mac[6] = {0x52, 0x54, 0, 0x12, 0x34, 0x57};
	static const u8 local_ip[4] = {10, 0, 2, 15};
	static const u8 peer_ip[4] = {10, 0, 2, 2};
	struct net_arp_state arp;
	struct net_ipv4_state ipv4;
	struct net_udp_state udp;
	u8 frame[NET_PACKET_BUFFER_SIZE];
	net_arp_init(&arp, local_mac, local_ip);
	net_ipv4_init(&ipv4, local_ip);
	net_udp_init(&udp, NET_UDP_ECHO_PORT);

	build_arp_request(frame, peer_mac, peer_ip, local_ip);
	clear_sent();
	if (check(net_arp_handle_frame(&arp, frame, NET_ETHERNET_MIN_FRAME_LENGTH,
	                               test_send) == 2, "ARP reply")) return 1;
	if (check(sent_count == 1, "ARP reply send")) return 1;

	u32 length = build_icmp_request(frame, peer_mac, peer_ip,
	                               local_mac, local_ip, ipv4.ping_identifier);
	clear_sent();
	if (check(net_ipv4_handle_frame(&ipv4, &arp, frame, length,
	                                test_send) == 2, "ICMP reply")) return 1;
	if (check(sent_count == 1, "ICMP reply send")) return 1;

	if (check(net_udp_bind(&udp, 4000) == 0, "UDP bind")) return 1;
	length = build_udp_request(frame, peer_mac, peer_ip, local_mac,
	                           local_ip, 4000);
	if (check(net_udp_handle_frame(&udp, &arp, frame, length,
	                               test_send) == NET_NETWORK_POLL_UDP_QUEUED,
			"UDP queue")) return 1;
	struct net_udp_datagram datagram;
	if (check(net_udp_receive(&udp, 4000, &datagram) == 0,
			"UDP receive") || check(datagram.length == 4,
			"UDP receive length") || check(memcmp(datagram.payload, "echo", 4) == 0,
			"UDP receive payload")) return 1;

	struct net_dhcp_state dhcp;
	net_dhcp_init(&dhcp, local_mac, 0x5A49524F);
	clear_sent();
	if (check(net_dhcp_start(&dhcp, &arp, &udp, test_send) == 0,
			"DHCP discover")) return 1;
	build_dhcp_reply(&datagram, &dhcp, NET_DHCP_OFFER,
	                 (const u8[]){10, 0, 2, 20},
	                 (const u8[]){10, 0, 2, 1});
	if (check(net_dhcp_handle(&dhcp, &arp, &ipv4, &udp, &datagram,
			100, 1000, test_send) == NET_DHCP_OFFER_RECEIVED,
			"DHCP offer")) return 1;
	build_dhcp_reply(&datagram, &dhcp, NET_DHCP_ACK,
	                 (const u8[]){10, 0, 2, 20},
	                 (const u8[]){10, 0, 2, 1});
	if (check(net_dhcp_handle(&dhcp, &arp, &ipv4, &udp, &datagram,
			200, 1000, test_send) == NET_DHCP_BOUND,
			"DHCP bound") || check(dhcp.bound, "DHCP bound state") ||
	    check(ipv4.local_ip[3] == 20, "DHCP IPv4 address")) return 1;
	u32 renew_at = dhcp.renew_at;
	u32 rebind_at = dhcp.rebind_at;
	u32 expire_at = dhcp.expire_at;
	if (check(net_dhcp_tick(&dhcp, &arp, &ipv4, &udp, renew_at,
				test_send) == 0, "DHCP renewal") ||
	    check(dhcp.state == NET_DHCP_STATE_RENEWING,
			"DHCP renewing state") ||
	    check(dhcp.renewal_count == 1, "DHCP renewal count")) return 1;
	if (check(net_dhcp_tick(&dhcp, &arp, &ipv4, &udp, rebind_at,
				test_send) == 0, "DHCP rebinding") ||
	    check(dhcp.state == NET_DHCP_STATE_REBINDING,
			"DHCP rebinding state")) return 1;
	if (check(net_dhcp_tick(&dhcp, &arp, &ipv4, &udp, expire_at,
				 test_send) == -1, "DHCP expiry")) return 1;

	net_arp_init(&arp, local_mac, local_ip);
	net_ipv4_init(&ipv4, local_ip);
	struct net_tcp_state tcp;
	net_tcp_init(&tcp);
	if (check(net_tcp_listen(&tcp, 4001) == 0, "TCP listen")) return 1;
	static const u8 invalid_options[4] = {
		NET_TCP_OPTION_MSS, 5, 0, 128,
	};
	clear_sent();
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9002, 4001, 100, 0, NET_TCP_FLAG_SYN,
	                           0, 0, 0, 0);
	frame[NET_ETHERNET_HEADER_LENGTH + 8] ^= 1;
	int malformed_result = net_tcp_handle_frame(&tcp, &arp, frame, length, test_send);
	if (check(malformed_result == -2,
			"TCP invalid IP checksum")) return 1;
	clear_sent();
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9003, 4001, 100, 0, NET_TCP_FLAG_SYN,
	                           invalid_options, sizeof(invalid_options), 0, 0);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) == -4,
			"TCP invalid option length")) return 1;
	clear_sent();
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9004, 4001, 100, 0, NET_TCP_FLAG_SYN,
	                           0, 0, 0, 0);
	frame[NET_ETHERNET_HEADER_LENGTH + NET_IPV4_HEADER_LENGTH + 16] ^= 1;
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) == -3,
			"TCP invalid checksum")) return 1;
	clear_sent();
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9005, 4001, 100, 0, NET_TCP_FLAG_SYN,
	                           0, 0, 0, 0);
	struct net_tcp_header *bad_tcp =
		(struct net_tcp_header *)(frame + NET_ETHERNET_HEADER_LENGTH +
		                          NET_IPV4_HEADER_LENGTH);
	bad_tcp->data_offset_flags = net_be16((4 << 12) | NET_TCP_FLAG_SYN);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) == -3,
			"TCP invalid data offset")) return 1;
	clear_sent();
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9006, 4001, 100, 0, NET_TCP_FLAG_SYN,
	                           0, 0, 0, 0);
	struct net_ipv4_header *bad_ip =
		(struct net_ipv4_header *)(frame + NET_ETHERNET_HEADER_LENGTH);
	bad_ip->total_length = net_be16(NET_IPV4_HEADER_LENGTH - 1);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) == -2,
			"TCP invalid IP length")) return 1;
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9007, 4001, 100, 0,
	                           NET_TCP_FLAG_SYN | NET_TCP_FLAG_FIN,
	                           0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) == -11,
			"TCP invalid flag combination")) return 1;
	static const u8 syn_options[12] = {
		NET_TCP_OPTION_MSS, 4, 0, 128,
		NET_TCP_OPTION_NOP, NET_TCP_OPTION_WINDOW_SCALE, 3, 2,
		NET_TCP_OPTION_SACK_PERMITTED, 2, NET_TCP_OPTION_EOL,
		NET_TCP_OPTION_EOL,
	};
	clear_sent();
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9001, 4001, 100, 0, NET_TCP_FLAG_SYN,
	                           syn_options, sizeof(syn_options), 0, 0);
	int result = net_tcp_handle_frame(&tcp, &arp, frame, length, test_send);
	struct net_tcp_connection *connection = &tcp.connections[0];
	if (check(result == NET_NETWORK_POLL_TCP_ESTABLISHED, "TCP SYN") ||
	    check(tcp.malformed == 6, "TCP malformed counter") ||
	    check(tcp.checksum_errors == 2, "TCP checksum counter") ||
	    check(tcp.option_errors == 1, "TCP option counter") ||
	    check(connection->remote_mss == 128, "TCP MSS option") ||
	    check(connection->window_scale_enabled, "TCP window scale option") ||
	    check(connection->sack_permitted, "TCP SACK option") ||
	    check(tcp_has_option(sent_frame, NET_TCP_OPTION_WINDOW_SCALE),
			"TCP SYN-ACK options")) return 1;
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9001, 4001, 101, connection->send_sequence + 1,
	                           NET_TCP_FLAG_ACK, 0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) == -9,
			"TCP invalid ACK") || check(tcp.ack_errors == 1,
			"TCP ACK counter")) return 1;

	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9001, 4001, 101, connection->send_sequence,
	                           NET_TCP_FLAG_ACK, 0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) ==
			NET_NETWORK_POLL_TCP_ESTABLISHED, "TCP ACK")) return 1;
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9001, 4001, 101, connection->send_sequence,
	                           NET_TCP_FLAG_ACK, 0, 0, 0, 0);
	set_tcp_window(frame, 0);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) == 0,
			"TCP zero-window ACK") ||
	    check(net_tcp_send_data(&tcp, &arp, "x", 1, test_send) != 0,
			"TCP zero-window block")) return 1;
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9001, 4001, 101, connection->send_sequence,
	                           NET_TCP_FLAG_ACK, 0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) == 0,
			"TCP window update") || check(connection->remote_window != 0,
			"TCP window reopened")) return 1;

	clear_sent();
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9001, 4001, 106, connection->send_sequence,
	                           NET_TCP_FLAG_ACK | NET_TCP_FLAG_PSH, 0, 0,
	                           "world", 5);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) == 0,
			"TCP reorder queue") || check(connection->reorder_count == 1,
			"TCP reorder count") || check(tcp_has_option(sent_frame,
			NET_TCP_OPTION_SACK), "TCP SACK block")) return 1;
	clear_sent();
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9001, 4001, 101, connection->send_sequence,
	                           NET_TCP_FLAG_ACK | NET_TCP_FLAG_PSH, 0, 0,
	                           "hello", 5);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) ==
			NET_NETWORK_POLL_TCP_DATA, "TCP reordered data")) return 1;
	u8 received[16];
	u16 received_length = 0;
	if (check(net_tcp_receive_data(&tcp, received, sizeof(received),
			&received_length) == 0, "TCP receive") ||
	    check(received_length == 10, "TCP receive length") ||
	    check(memcmp(received, "helloworld", 10) == 0,
			"TCP reordered payload")) return 1;

	clear_sent();
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9001, 4001, 101, connection->send_sequence,
	                           NET_TCP_FLAG_ACK | NET_TCP_FLAG_PSH, 0, 0,
	                           "hello", 5);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) == 0,
			"TCP duplicate data") || check(sent_count == 1,
			"TCP duplicate data ACK")) return 1;

	u32 first_send_sequence = connection->send_sequence;
	if (check(net_tcp_send_data(&tcp, &arp, "world", 5, test_send) == 0,
			"TCP send data") || check(net_tcp_send_data(&tcp, &arp, "again", 5,
			test_send) == 0, "TCP outstanding data")) return 1;
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9001, 4001, 111, first_send_sequence,
	                           NET_TCP_FLAG_ACK, 0, 0, 0, 0);
	set_tcp_window(frame, 0);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) == 0,
			"TCP pending zero-window") || check(connection->remote_window == 0,
			"TCP pending window zero")) return 1;
	clear_sent();
	if (check(net_tcp_tick(&tcp, &arp,
			tcp.now + NET_TCP_PERSIST_TICKS, test_send) == 1,
			"TCP persist probe")) return 1;
	struct net_ipv4_header *probe_ip =
		(struct net_ipv4_header *)(sent_frame + NET_ETHERNET_HEADER_LENGTH);
	if (check(net_be16(probe_ip->total_length) ==
			NET_IPV4_HEADER_LENGTH + NET_TCP_HEADER_LENGTH + 1,
			"TCP persist probe length")) return 1;
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9001, 4001, 111, first_send_sequence + 5,
	                           NET_TCP_FLAG_ACK, 0, 0, 0, 0);
	clear_sent();
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) == 0,
			"TCP window reopen after persist")) return 1;
	clear_sent();
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9001, 4001, 111, first_send_sequence + 5,
	                           NET_TCP_FLAG_ACK, 0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) == 0,
			"TCP cumulative ACK") || check(connection->tx_count == 1,
			"TCP cumulative queue")) return 1;
	clear_sent();
	for (u32 index = 0; index < 3; index++)
		if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) == 0,
			"TCP duplicate ACK")) return 1;
	if (check(sent_count == 1, "TCP fast retransmit") ||
	    check(tcp.fast_retransmits >= 1, "TCP fast retransmit counter")) return 1;

	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9001, 4001, 111, connection->send_sequence,
	                           NET_TCP_FLAG_FIN | NET_TCP_FLAG_ACK, 0, 0,
	                           0, 0);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) ==
			NET_NETWORK_POLL_TCP_FIN, "TCP CLOSE-WAIT FIN") ||
	    check(connection->state == NET_TCP_STATE_CLOSE_WAIT,
			"TCP CLOSE-WAIT")) return 1;
	if (check(net_tcp_close(&tcp, &arp, test_send) == 0,
			"TCP LAST-ACK") || check(connection->state == NET_TCP_STATE_LAST_ACK,
			"TCP LAST-ACK state")) return 1;
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9001, 4001, 111, connection->send_sequence,
	                           NET_TCP_FLAG_ACK, 0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) ==
			NET_NETWORK_POLL_TCP_CLOSED, "TCP LAST-ACK cleanup")) return 1;

	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9002, 4001, 200, 0, NET_TCP_FLAG_SYN,
	                           0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) ==
			NET_NETWORK_POLL_TCP_ESTABLISHED, "TCP active-close SYN")) return 1;
	connection = &tcp.connections[0];
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9002, 4001, 201, connection->send_sequence,
	                           NET_TCP_FLAG_ACK, 0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) ==
			NET_NETWORK_POLL_TCP_ESTABLISHED, "TCP active-close ACK")) return 1;
	if (check(net_tcp_close(&tcp, &arp, test_send) == 0,
			"TCP FIN-WAIT-1") ||
	    check(connection->state == NET_TCP_STATE_FIN_WAIT_1,
			"TCP FIN-WAIT-1 state")) return 1;
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9002, 4001, 201, connection->send_sequence,
	                           NET_TCP_FLAG_ACK, 0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) == 0,
			"TCP FIN-WAIT-2 ACK") ||
	    check(connection->state == NET_TCP_STATE_FIN_WAIT_2,
			"TCP FIN-WAIT-2 state")) return 1;
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9002, 4001, 201, connection->send_sequence,
	                           NET_TCP_FLAG_FIN | NET_TCP_FLAG_ACK, 0, 0,
	                           0, 0);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) ==
			NET_NETWORK_POLL_TCP_FIN, "TCP TIME-WAIT FIN") ||
	    check(connection->state == NET_TCP_STATE_TIME_WAIT,
			"TCP TIME-WAIT state")) return 1;
	u32 time_wait_at = connection->time_wait_at;
	if (check(net_tcp_tick(&tcp, &arp, time_wait_at, test_send) == 1,
			"TCP TIME-WAIT cleanup") || check(!connection->used,
			"TCP TIME-WAIT released")) return 1;

	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9003, 4001, 300, 0, NET_TCP_FLAG_SYN,
	                           0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) ==
			NET_NETWORK_POLL_TCP_ESTABLISHED, "TCP persist budget SYN")) return 1;
	connection = &tcp.connections[0];
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9003, 4001, 301, connection->send_sequence,
	                           NET_TCP_FLAG_ACK, 0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) ==
			NET_NETWORK_POLL_TCP_ESTABLISHED, "TCP persist budget ACK")) return 1;
	u32 persist_sequence = connection->send_sequence;
	if (check(net_tcp_send_data(&tcp, &arp, "z", 1, test_send) == 0,
			"TCP persist budget data")) return 1;
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9003, 4001, 301, persist_sequence,
	                           NET_TCP_FLAG_ACK, 0, 0, 0, 0);
	set_tcp_window(frame, 0);
	if (check(net_tcp_handle_frame(&tcp, &arp, frame, length, test_send) == 0,
			"TCP persist budget zero-window")) return 1;
	u32 persist_now = tcp.now + NET_TCP_PERSIST_TICKS;
	for (u32 index = 0; index < NET_TCP_MAX_RETRANSMITS; index++) {
		clear_sent();
		int persist_result = net_tcp_tick(&tcp, &arp, persist_now, test_send);
		if (check(persist_result == 1,
				"TCP persist budget probe") ||
		    check(sent_count == 1, "TCP persist budget frame")) return 1;
		persist_now += NET_TCP_PERSIST_TICKS;
	}
	clear_sent();
	if (check(net_tcp_tick(&tcp, &arp, persist_now, test_send) == 1,
			"TCP persist budget cleanup") || check(!connection->used,
			"TCP persist budget released") ||
	    check(tcp.persist_probes == 4, "TCP persist probe counter") ||
	    check(tcp.persist_expired == 1, "TCP persist expiry counter")) return 1;

	struct net_tcp_state edge_tcp;
	net_tcp_init(&edge_tcp);
	if (check(net_tcp_listen(&edge_tcp, 4001) == 0, "TCP edge listen")) return 1;
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9010, 4001, 400, 0, NET_TCP_FLAG_SYN,
	                           0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&edge_tcp, &arp, frame, length,
			test_send) == NET_NETWORK_POLL_TCP_ESTABLISHED,
			"TCP edge SYN")) return 1;
	struct net_tcp_connection *edge_connection = &edge_tcp.connections[0];
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9010, 4001, 401, edge_connection->send_sequence,
	                           NET_TCP_FLAG_ACK, 0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&edge_tcp, &arp, frame, length,
			test_send) == NET_NETWORK_POLL_TCP_ESTABLISHED,
			"TCP edge ACK")) return 1;
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9010, 4001, 401, edge_connection->send_sequence,
	                           NET_TCP_FLAG_RST | NET_TCP_FLAG_ACK, 0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&edge_tcp, &arp, frame, length,
			test_send) == NET_NETWORK_POLL_TCP_CLOSED,
			"TCP established RST") || check(!edge_connection->used,
			"TCP established RST cleanup")) return 1;
	if (check(net_tcp_handle_frame(&edge_tcp, &arp, frame, length,
			test_send) == 0, "TCP stale ACK after cleanup") ||
	    check(!edge_connection->used, "TCP stale ACK no allocation")) return 1;

	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9011, 4001, 500, 0, NET_TCP_FLAG_SYN,
	                           0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&edge_tcp, &arp, frame, length,
			test_send) == NET_NETWORK_POLL_TCP_ESTABLISHED,
			"TCP SYN_RECEIVED RST SYN")) return 1;
	edge_connection = &edge_tcp.connections[0];
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9011, 4001, 501, 0, NET_TCP_FLAG_RST,
	                           0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&edge_tcp, &arp, frame, length,
			test_send) == NET_NETWORK_POLL_TCP_CLOSED,
			"TCP SYN_RECEIVED RST") || check(!edge_connection->used,
			"TCP SYN_RECEIVED RST cleanup")) return 1;

	struct net_tcp_state wrap_tcp;
	net_tcp_init(&wrap_tcp);
	if (check(net_tcp_listen(&wrap_tcp, 4001) == 0, "TCP wrap listen")) return 1;
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9012, 4001, 0xFFFFFFFEu, 0,
	                           NET_TCP_FLAG_SYN, 0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&wrap_tcp, &arp, frame, length,
			test_send) == NET_NETWORK_POLL_TCP_ESTABLISHED,
			"TCP wrap SYN")) return 1;
	struct net_tcp_connection *wrap_connection = &wrap_tcp.connections[0];
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9012, 4001, 0xFFFFFFFFu,
	                           wrap_connection->send_sequence,
	                           NET_TCP_FLAG_ACK, 0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&wrap_tcp, &arp, frame, length,
			test_send) == NET_NETWORK_POLL_TCP_ESTABLISHED,
			"TCP wrap ACK")) return 1;
	wrap_connection->send_sequence = 0xFFFFFFFEu;
	wrap_connection->last_acknowledgement = 0xFFFFFFFEu;
	if (check(net_tcp_send_data(&wrap_tcp, &arp, "abcd", 4, test_send) == 0,
			"TCP wrap send")) return 1;
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9012, 4001, 0xFFFFFFFFu, 2,
	                           NET_TCP_FLAG_ACK, 0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&wrap_tcp, &arp, frame, length,
			test_send) == 0, "TCP wrap ACK data") ||
	    check(wrap_connection->tx_count == 0, "TCP wrap ACK cleanup")) return 1;
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9012, 4001, 0xFFFFFFFFu, 2,
	                           NET_TCP_FLAG_ACK | NET_TCP_FLAG_PSH,
	                           0, 0, "xy", 2);
	if (check(net_tcp_handle_frame(&wrap_tcp, &arp, frame, length,
			test_send) == NET_NETWORK_POLL_TCP_DATA,
			"TCP wrap receive")) return 1;
	received_length = 0;
	if (check(net_tcp_receive_data(&wrap_tcp, received, sizeof(received),
			&received_length) == 0, "TCP wrap receive data") ||
	    check(received_length == 2, "TCP wrap receive length") ||
	    check(memcmp(received, "xy", 2) == 0, "TCP wrap receive payload")) return 1;
	length = build_tcp_request(frame, peer_mac, peer_ip, local_mac, local_ip,
	                           9012, 4001, 1, 2,
	                           NET_TCP_FLAG_RST | NET_TCP_FLAG_ACK, 0, 0, 0, 0);
	if (check(net_tcp_handle_frame(&wrap_tcp, &arp, frame, length,
			test_send) == NET_NETWORK_POLL_TCP_CLOSED,
			"TCP wrap RST cleanup")) return 1;

	printf("network protocol tests: OK\n");
	return 0;
}
