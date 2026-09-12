short *video = (short*)0xB8000;


#include "config.h"
#include "int_types.h"
bool dont_interrupt_me = false;
u32 ticks = 0;
#include "ports.h"
#include "serial.h"
#include "print.h"
#include "allocator.h"
#include "interrupt.h"
#include "cpu.h"
#include "process.h"
#include "mouse.h"
#include "fb.h"
#include "keyboard.h"
#include "function_manager.h"
#include "shell.h"
#include "time.h"
#include "disk.h"
#include "ramdisk.h"
#include "neuro_ide.h"
#include "pci.h"
#include "fat16.h"
#include "rand.h"
#include "acpi.h"
#include "bios.h"
#include "vfs.h"
#include "fpu.h"
#include "drivers/e1000.h"
#include "net/ethernet.h"
#include "net/arp.h"
#include "net/ipv4.h"
#include "net/tcp.h"
#include "net/udp.h"
#include "net/dhcp.h"

struct object *echo(struct objectArray* args)
{
	for(int i = 0; i<args->count;i++)
	{
		if(args->objs[i].type==OBJECT_STRING){
			print(args->objs[i].data);
			print(" ");
		}
	}
	print("\n");
	return 0;
}

struct object *clear(struct objectArray* args)
{
	cls();
	return 0;
}

char * fetch_logo[]={
	"         _.--._",
	"    _.-.'      `.-._",
	"  .' ./`--...--'\\   `.",
	"  `.'.`--.._..--'   .'",
	"    `-..__    __..-'",
	"          ````",
};

struct object *screenfetch(struct objectArray* args)
{
	u32 used=0, freemem=0;
	struct memoryblock *current = headblock.next;
	while(current)
	{	
		if(current->allocated) {
			used+=current->size;
		}
		else {
			freemem+=current->size;
		}
		current = current->next;
	}
	
	print_color(fetch_logo[0],0b1110);
	for(int i = strlen(fetch_logo[0]);i<25;i++) putchar(' ');
	print_color("OS: ",0b0011);
	print("ZHIROS\n");

	print_color(fetch_logo[1],0b1110);
	for(int i = strlen(fetch_logo[1]);i<25;i++) putchar(' ');
	
	print_color("Uptime: ",0b0011);
	u32 all = ticks/1000;
        u32 sec = all%60;
        all/=60;
        u32 min = all%60;
        all/=60;
        u32 hour = all;

        if(hour>0){print_int(hour);print("h ");}
        if(min>0){print_int(min);print("m ");}
        print_int(sec);print("s\n");
	

	print_color(fetch_logo[2],0b1110);
	for(int i = strlen(fetch_logo[2]);i<25;i++) putchar(' ');
	
	print_color("Display: ", 0b0011);
	print_int(screen_width);
	print("x");
	print_int(screen_height);
	print("\n");
	
	print_color(fetch_logo[3],0b1110);
	for(int i = strlen(fetch_logo[3]);i<25;i++) putchar(' ');
	

	char *cpu_model = kalloc(64);
        get_cpu_model(cpu_model);
        
	print_color("CPU: ", 0b0011);
	print(cpu_model);
	print("\n");
	free(cpu_model);	

	print_color(fetch_logo[4],0b1110);
	for(int i = strlen(fetch_logo[4]);i<25;i++) putchar(' ');
	
	print_color("Memory:", 0b0011);
	print_int(used>>10);
	print("k/");
	print_int((used+freemem)>>20);
	print("M\n");

	print_color(fetch_logo[5],0b1110);
	for(int i = strlen(fetch_logo[5]);i<25;i++) putchar(' ');
	print("\n");

	for(int i = 0;i<25;i++)putchar(' ');
	for(int i = 0;i<16;i++)print_color(" ",i<<4);
	print("\n");
	return 0;
}

struct object *date(struct objectArray* args)
{
	struct rtc_time now = get_time();

        print_int(now.day);
	print(".");
	print_int(now.month);
	print(".");
	print_int(now.year+2000);
	
	print("   ");
	
	print_int(now.hour);
        print(":");
        print_int(now.minute);
        print(":");
        print_int(now.second);
        print("\n");

	return 0;
}

struct object *help(struct objectArray* args)
{
	struct function_info *current = head_fnc;
	while(current){
		print(current->name);
		for(int i = strlen(current->name);i<20;i++) putchar(' ');
		print_color(current->description,0b0011);
		print("\n");
		current = current->next;
	}
	print("windows manager     ");
	print_color("press ctrl+num to select window\n",0b0101);
	return 0;
}

struct object *lsblk(struct objectArray* args)
{ 
	print("NAME                CODE    SIZE\n");

	for(int i = 0;i<MAX_DISK_COUNT;i++)
	{
		if(!disks[i])continue;
		print(disks[i]->name);
		for(int j = strlen(disks[i]->name);j<20;j++) putchar(' ');
		putchar(i+'a');
		print("       ");
		int sizeinmb = disks[i]->size>>11;
		if(sizeinmb>1024)
		{
			print_int(sizeinmb>>10);
			print("G\n");
		}else{
			print_int(sizeinmb);
			print("MB\n");
		}
		for(int j = 0;j<4;j++)
		{
			if(disks[i]->partitions[j].total_sectors==0)continue;
			print("  |--part");
			print_int(j);
			print("          ");
			putchar(i+'a');
			print_int(j);
			print("      ");
			sizeinmb = disks[i]->partitions[j].total_sectors>>11;
			if(sizeinmb>1024)
                	{       
                        	print_int(sizeinmb>>10);
                        	print("G\n");
                	}else{
                        	print_int(sizeinmb);
                        	print("MB\n");
                	}

		}
	}
	return 0;
}

struct object *dump_disk(struct objectArray*args)
{
	if(args->count<3){
		KLOGE("use dmpdsk <disk letter> <lba> <len>\n");
		return 0;
	}
	int idx = *(char*)args->objs[0].data-'a';
	int lba = str2int(args->objs[1].data);
	int len = str2int(args->objs[2].data);
	int blocks = len/512;
	if(len%512)blocks+=1;
	
	char *buffer = kalloc(blocks*512);
	disks[idx]->lba_read(disks[idx]->id,lba,buffer,blocks);
	
	hexdump(buffer,len);	
	free(buffer);
	return 0;
}

struct object *lspci(struct objectArray* args)
{
	pci_scan();
}

struct object *netinfo(struct objectArray* args)
{
	e1000_print_info();
	return 0;
}

static u8 net_rx_frame[E1000_PACKET_BUFFER_SIZE];
static struct net_arp_state net_arp_state;
static struct net_ipv4_state net_ipv4_state;
static struct net_tcp_state net_tcp_state;
static struct net_udp_state net_udp_state;
static struct net_dhcp_state net_dhcp_state;
static struct net_udp_datagram net_received_datagram;
static u8 net_tcp_receive_buffer[NET_TCP_RECV_BUFFER_SIZE];
static const u8 net_local_ip[4] = {10, 0, 2, 15};
static const u8 net_gateway_ip[4] = {10, 0, 2, 2};
#define NET_ARP_WAIT_TICKS 100
#define NET_PING_RETRIES 3
#define NET_DHCP_WAIT_TICKS 200
#define NET_DHCP_RETRIES 3
static volatile u32 net_lock;

static int net_lock_acquire()
{
	asm volatile("cli");
	if (net_lock) {
		asm volatile("sti");
		return 0;
	}
	net_lock = 1;
	asm volatile("sti");
	return 1;
}

static void net_lock_release()
{
	asm volatile("cli");
	net_lock = 0;
	asm volatile("sti");
}

static int net_command_target(struct objectArray *args, u8 *target)
{
	if (!args || !target || args->count > 1) return -1;
	if (!args->count) {
		memcpy(target, net_gateway_ip, 4);
		return 0;
	}
	return net_parse_ipv4(args->objs[0].data, target);
}

static int net_wait_for_arp(const u8 *target, u32 timeout)
{
	u32 start = ticks;
	while ((u32)(ticks - start) < timeout) {
		if (net_lock_acquire()) {
			u8 mac[NET_ETHERNET_ADDRESS_LENGTH];
			int result = net_arp_lookup(&net_arp_state, target, mac);
			net_lock_release();
			if (result == 0) return 0;
		}
		asm volatile("hlt");
	}
	return -1;
}

static int net_wait_for_echo(u32 previous, u32 timeout)
{
	u32 start = ticks;
	while ((u32)(ticks - start) < timeout) {
		if (net_lock_acquire()) {
			u32 received = net_ipv4_state.echo_received;
			net_lock_release();
			if (received != previous) return 0;
		}
		asm volatile("hlt");
	}
	return -1;
}

struct object *nettx(struct objectArray* args)
{
	if (!net_lock_acquire()) {
		print("network busy\n");
		return 0;
	}
	int result = e1000_send_test_frame();
	net_lock_release();
	if (result == 0) print("E1000 TX: complete\n");
	else {
		print("E1000 TX: error ");
		print_int(result);
		print("\n");
	}
	return 0;
}

struct object *netrx(struct objectArray* args)
{
	if (!net_lock_acquire()) {
		print("network busy\n");
		return 0;
	}
	u32 length = 0;
	int result = e1000_receive_frame(net_rx_frame, sizeof(net_rx_frame), &length);
	net_lock_release();
	if (result == 1) print("E1000 RX: no frame\n");
	else if (result < 0) {
		print("E1000 RX: error ");
		print_int(result);
		print("\n");
	} else {
		print("E1000 RX: frame length ");
		print_int(length);
		print("\n");
	}
	return 0;
}

struct object *netarp(struct objectArray* args)
{
	u8 target[4];
	int parse = net_command_target(args, target);
	if (parse) {
		print("use netarp [ipv4]\n");
		return 0;
	}
	if (!net_lock_acquire()) {
		print("network busy\n");
		return 0;
	}
	int result = net_arp_request(&net_arp_state, target, e1000_send_frame);
	net_lock_release();
	print("ARP request: ");
	print(result == 0 ? "sent\n" : "error\n");
	return 0;
}

struct object *netping(struct objectArray* args)
{
	u8 target[4];
	int parse = net_command_target(args, target);
	if (parse) {
		print("use netping [ipv4]\n");
		return 0;
	}
	int last_error = -1;
	for (u32 attempt = 0; attempt < NET_PING_RETRIES; attempt++) {
		if (attempt) {
			if (!net_lock_acquire()) {
				print("network busy\n");
				return 0;
			}
			net_arp_invalidate(&net_arp_state, target);
			net_lock_release();
		}
		if (!net_lock_acquire()) {
			print("network busy\n");
			return 0;
		}
		u32 previous = net_ipv4_state.echo_received;
		int result = net_ipv4_send_echo(&net_ipv4_state, &net_arp_state,
		                               target, e1000_send_frame);
		net_lock_release();
		if (result == 1) {
			if (net_wait_for_arp(target, NET_ARP_WAIT_TICKS)) {
				last_error = -2;
				continue;
			}
			if (!net_lock_acquire()) {
				print("network busy\n");
				return 0;
			}
			previous = net_ipv4_state.echo_received;
			result = net_ipv4_send_echo(&net_ipv4_state, &net_arp_state,
			                           target, e1000_send_frame);
			net_lock_release();
		}
		if (result) {
			last_error = result;
			continue;
		}
		if (!net_wait_for_echo(previous, NET_ARP_WAIT_TICKS)) {
			print("ICMP echo reply: received\n");
			return 0;
		}
		last_error = -3;
	}
	print("ICMP echo: timeout/error ");
	print_int(last_error);
	print("\n");
	return 0;
}

struct object *netudp(struct objectArray* args)
{
	if (!args || args->count > 2) {
		print("use netudp [ipv4] [port]\n");
		return 0;
	}
	u8 target[4];
	if (net_command_target(args, target) && args->count != 2) {
		print("use netudp [ipv4] [port]\n");
		return 0;
	}
	if (args->count == 2 && net_parse_ipv4(args->objs[0].data, target)) {
		print("use netudp [ipv4] [port]\n");
		return 0;
	}
	u16 port = NET_UDP_ECHO_PORT;
	if (args->count == 2 && net_parse_u16(args->objs[1].data, &port)) {
		print("use netudp [ipv4] [port]\n");
		return 0;
	}
	if (!net_lock_acquire()) {
		print("network busy\n");
		return 0;
	}
	static const u8 payload[] = "zhiros udp";
	int result = net_udp_send(&net_udp_state, &net_arp_state,
	                          target, port, payload, sizeof(payload) - 1,
	                          e1000_send_frame);
	net_lock_release();
	if (result == 1) {
		if (!net_wait_for_arp(target, 100)) {
			if (!net_lock_acquire()) {
				print("network busy\n");
				return 0;
			}
			result = net_udp_send(&net_udp_state, &net_arp_state,
			                     target, port, payload, sizeof(payload) - 1,
			                     e1000_send_frame);
			net_lock_release();
		} else {
			print("UDP datagram: ARP timeout\n");
			return 0;
		}
	}
	if (result == 0) print("UDP datagram: sent\n");
	else {
		print("UDP datagram: error ");
		print_int(result);
		print("\n");
	}
	return 0;
}

struct object *dhcp_cmd(struct objectArray* args)
{
	if (args && args->count) {
		print("use dhcp\n");
		return 0;
	}
	for (u32 attempt = 0; attempt < NET_DHCP_RETRIES; attempt++) {
		if (!net_lock_acquire()) {
			print("network busy\n");
			return 0;
		}
		net_udp_unbind(&net_udp_state, NET_DHCP_CLIENT_PORT);
		net_dhcp_init(&net_dhcp_state, net_arp_state.local_mac,
		              0x5A49524F + attempt);
		net_arp_clear_cache(&net_arp_state);
		int result = net_dhcp_start(&net_dhcp_state, &net_arp_state,
		                            &net_udp_state, e1000_send_frame);
		net_lock_release();
		if (result) {
			print("DHCP discover error ");
			print_int(result);
			print("\n");
			return 0;
		}
		u32 start = ticks;
		while ((u32)(ticks - start) < NET_DHCP_WAIT_TICKS) {
			if (!net_lock_acquire()) {
				asm volatile("hlt");
				continue;
			}
			result = net_dhcp_poll(&net_dhcp_state, &net_arp_state,
			                       &net_ipv4_state, &net_udp_state,
			                       ticks, 1000, e1000_send_frame);
			u32 dhcp_bound = net_dhcp_state.bound;
			net_lock_release();
			if (result == NET_DHCP_BOUND || dhcp_bound) {
				print("DHCP bound ");
				for (u32 index = 0; index < 4; index++) {
					if (index) print(".");
					print_int(net_dhcp_state.offered_ip[index]);
				}
				print(" lease ");
				print_int(net_dhcp_state.lease_time);
				print("\n");
				return 0;
			}
			if (result < 0) break;
			if (result == NET_DHCP_OFFER_RECEIVED) start = ticks;
			asm volatile("hlt");
		}
		print("DHCP retry\n");
	}
	print("DHCP timeout\n");
	return 0;
}

struct object *udplisten(struct objectArray* args)
{
	u16 port = 0;
	if (!args || args->count != 1 ||
	    net_parse_u16(args->objs[0].data, &port)) {
		print("use udplisten <port>\n");
		return 0;
	}
	if (!net_lock_acquire()) {
		print("network busy\n");
		return 0;
	}
	int result = net_udp_bind(&net_udp_state, port);
	net_lock_release();
	if (!result) print("UDP listen: bound\n");
	else {
		print("UDP listen: error ");
		print_int(result);
		print("\n");
	}
	return 0;
}

struct object *udpunbind(struct objectArray* args)
{
	u16 port = 0;
	if (!args || args->count != 1 ||
	    net_parse_u16(args->objs[0].data, &port)) {
		print("use udpunbind <port>\n");
		return 0;
	}
	if (!net_lock_acquire()) {
		print("network busy\n");
		return 0;
	}
	int result = net_udp_unbind(&net_udp_state, port);
	net_lock_release();
	if (!result) print("UDP listen: unbound\n");
	else {
		print("UDP unbind: error ");
		print_int(result);
		print("\n");
	}
	return 0;
}

struct object *udprecv(struct objectArray* args)
{
	u16 port = 0;
	if (!args || args->count != 1 ||
	    net_parse_u16(args->objs[0].data, &port)) {
		print("use udprecv <port>\n");
		return 0;
	}
	if (!net_lock_acquire()) {
		print("network busy\n");
		return 0;
	}
	int result = net_udp_receive(&net_udp_state, port,
	                             &net_received_datagram);
	net_lock_release();
	if (result == 1) {
		print("UDP receive: no datagram\n");
		return 0;
	}
	if (result) {
		print("UDP receive: error ");
		print_int(result);
		print("\n");
		return 0;
	}
	print("UDP from ");
	for (u32 index = 0; index < 4; index++) {
		if (index) print(".");
		print_int(net_received_datagram.source_ip[index]);
	}
	print(":");
	print_int(net_received_datagram.source_port);
	print(" length ");
	print_int(net_received_datagram.length);
	print("\n");
	hexdump((char *)net_received_datagram.payload,
	        net_received_datagram.length);
	return 0;
}

struct object *udpsend(struct objectArray* args)
{
	if (!args || args->count != 3) {
		print("use udpsend <ipv4> <port> <text>\n");
		return 0;
	}
	u8 target[4];
	u16 port = 0;
	if (net_parse_ipv4(args->objs[0].data, target) ||
	    net_parse_u16(args->objs[1].data, &port) ||
	    args->objs[2].len > NET_UDP_PAYLOAD_MAX) {
		print("use udpsend <ipv4> <port> <text>\n");
		return 0;
	}
	if (!net_lock_acquire()) {
		print("network busy\n");
		return 0;
	}
	int result = net_udp_send_from(&net_udp_state, &net_arp_state,
	                              net_udp_state.port, target, port,
	                              args->objs[2].data, args->objs[2].len,
	                              e1000_send_frame);
	net_lock_release();
	if (result == 1) {
		if (net_wait_for_arp(target, NET_ARP_WAIT_TICKS)) {
			print("UDP send: ARP timeout\n");
			return 0;
		}
		if (!net_lock_acquire()) {
			print("network busy\n");
			return 0;
		}
		result = net_udp_send_from(&net_udp_state, &net_arp_state,
		                           net_udp_state.port, target, port,
		                           args->objs[2].data, args->objs[2].len,
		                           e1000_send_frame);
		net_lock_release();
	}
	if (!result) print("UDP send: sent\n");
	else {
		print("UDP send: error ");
		print_int(result);
		print("\n");
	}
	return 0;
}

struct object *tcplisten(struct objectArray* args)
{
	u16 port = 0;
	if (!args || args->count != 1 ||
	    net_parse_u16(args->objs[0].data, &port)) {
		print("use tcplisten <port>\n");
		return 0;
	}
	if (!net_lock_acquire()) {
		print("network busy\n");
		return 0;
	}
	int result = net_tcp_listen(&net_tcp_state, port);
	net_lock_release();
	if (!result) print("TCP listen: bound\n");
	else {
		print("TCP listen: error ");
		print_int(result);
		print("\n");
	}
	return 0;
}

struct object *tcpconnect(struct objectArray* args)
{
	if (!args || args->count != 2) {
		print("use tcpconnect <ipv4> <port>\n");
		return 0;
	}
	u8 target[4];
	u16 port = 0;
	if (net_parse_ipv4(args->objs[0].data, target) ||
	    net_parse_u16(args->objs[1].data, &port)) {
		print("use tcpconnect <ipv4> <port>\n");
		return 0;
	}
	if (!net_lock_acquire()) {
		print("network busy\n");
		return 0;
	}
	int result = net_tcp_connect(&net_tcp_state, &net_arp_state,
	                            target, port, e1000_send_frame);
	net_lock_release();
	if (result == 1) {
		if (net_wait_for_arp(target, NET_ARP_WAIT_TICKS)) {
			print("TCP connect: ARP timeout\n");
			return 0;
		}
		if (!net_lock_acquire()) {
			print("network busy\n");
			return 0;
		}
		result = net_tcp_connect(&net_tcp_state, &net_arp_state,
		                        target, port, e1000_send_frame);
		net_lock_release();
	}
	if (!result) print("TCP SYN: sent\n");
	else {
		print("TCP connect: error ");
		print_int(result);
		print("\n");
	}
	return 0;
}

struct object *tcpsend(struct objectArray* args)
{
	if (!args || args->count != 1 || args->objs[0].len > NET_TCP_PAYLOAD_MAX) {
		print("use tcpsend <text>\n");
		return 0;
	}
	if (!net_lock_acquire()) {
		print("network busy\n");
		return 0;
	}
	int result = net_tcp_send_data(&net_tcp_state, &net_arp_state,
	                              args->objs[0].data, args->objs[0].len,
	                              e1000_send_frame);
	net_lock_release();
	if (!result) print("TCP data: sent\n");
	else {
		print("TCP send: error ");
		print_int(result);
		print("\n");
	}
	return 0;
}

struct object *tcprecv(struct objectArray* args)
{
	if (args && args->count) {
		print("use tcprecv\n");
		return 0;
	}
	if (!net_lock_acquire()) {
		print("network busy\n");
		return 0;
	}
	u16 length = 0;
	int result = net_tcp_receive_data(&net_tcp_state, net_tcp_receive_buffer,
	                                  sizeof(net_tcp_receive_buffer), &length);
	net_lock_release();
	if (result == 1) {
		print("TCP receive: no data\n");
		return 0;
	}
	if (result) {
		print("TCP receive: error ");
		print_int(result);
		print("\n");
		return 0;
	}
	print("TCP data length ");
	print_int(length);
	print("\n");
	hexdump((char *)net_tcp_receive_buffer, length);
	return 0;
}

struct object *tcpclose(struct objectArray* args)
{
	if (args && args->count) {
		print("use tcpclose\n");
		return 0;
	}
	if (!net_lock_acquire()) {
		print("network busy\n");
		return 0;
	}
	int result = net_tcp_close(&net_tcp_state, &net_arp_state,
	                          e1000_send_frame);
	net_lock_release();
	if (!result) print("TCP FIN: sent\n");
	else {
		print("TCP close: error ");
		print_int(result);
		print("\n");
	}
	return 0;
}

struct object *tcpinfo(struct objectArray* args)
{
	if (args && args->count) {
		print("use tcpinfo\n");
		return 0;
	}
	if (!net_lock_acquire()) {
		print("network busy\n");
		return 0;
	}
	struct net_tcp_connection *connection =
		net_tcp_first_active(&net_tcp_state);
	u32 connection_used = connection ? connection->used : 0;
	u32 connection_state = connection ? connection->state : 0;
	u16 local_port = connection ? connection->local_port : 0;
	u16 remote_port = connection ? connection->remote_port : 0;
	u32 congestion_window = connection ? connection->congestion_window : 0;
	u32 slow_start_threshold = connection ? connection->slow_start_threshold : 0;
	u32 remote_window = connection ? connection->remote_window : 0;
	u16 remote_mss = connection ? connection->remote_mss : 0;
	u32 tx_count = connection ? connection->tx_count : 0;
	u32 reorder_count = connection ? connection->reorder_count : 0;
	u32 duplicate_ack_count = connection ? connection->duplicate_ack_count : 0;
	u32 retransmissions = connection ? connection->retransmissions : 0;
	u32 fast_retransmits = connection ? connection->fast_retransmits : 0;
	u32 sack_blocks = connection ? connection->sack_blocks : 0;
	u32 persist_probes = connection ? connection->persist_probes : 0;
	u32 established = net_tcp_state.established;
	u32 received = net_tcp_state.received;
	u32 sent = net_tcp_state.sent;
	u32 closed = net_tcp_state.closed;
	u32 malformed = net_tcp_state.malformed;
	u32 checksum_errors = net_tcp_state.checksum_errors;
	u32 option_errors = net_tcp_state.option_errors;
	u32 ack_errors = net_tcp_state.ack_errors;
	u32 total_retransmissions = net_tcp_state.retransmissions;
	u32 total_fast_retransmits = net_tcp_state.fast_retransmits;
	u32 total_sack_blocks = net_tcp_state.sack_blocks;
	u32 total_persist_probes = net_tcp_state.persist_probes;
	u32 persist_expired = net_tcp_state.persist_expired;
	net_lock_release();
	print("TCP state:");
	print_int(connection_used ? connection_state : NET_TCP_STATE_CLOSED);
	print(" local:");
	print_int(local_port);
	print(" remote:");
	print_int(remote_port);
	print(" cwnd:");
	print_int(congestion_window);
	print(" ssthresh:");
	print_int(slow_start_threshold);
	print(" remote_window:");
	print_int(remote_window);
	print(" remote_mss:");
	print_int(remote_mss);
	print(" txq:");
	print_int(tx_count);
	print(" reorder:");
	print_int(reorder_count);
	print(" dupack:");
	print_int(duplicate_ack_count);
	print("\n");
	print("TCP counters tx:");
	print_int(sent);
	print(" rx:");
	print_int(received);
	print(" established:");
	print_int(established);
	print(" closed:");
	print_int(closed);
	print(" retransmit:");
	print_int(retransmissions);
	print(" fast:");
	print_int(fast_retransmits);
	print(" sack:");
	print_int(sack_blocks);
	print(" persist:");
	print_int(persist_probes);
	print("\n");
	print("TCP malformed:");
	print_int(malformed);
	print(" checksum:");
	print_int(checksum_errors);
	print(" options:");
	print_int(option_errors);
	print(" ack:");
	print_int(ack_errors);
	print(" total_retransmit:");
	print_int(total_retransmissions);
	print(" total_fast:");
	print_int(total_fast_retransmits);
	print(" total_sack:");
	print_int(total_sack_blocks);
	print(" total_persist:");
	print_int(total_persist_probes);
	print(" persist_expired:");
	print_int(persist_expired);
	print("\n");
	return 0;
}

struct object *netpoll(struct objectArray* args)
{
	if (!net_lock_acquire()) {
		print("network busy\n");
		return 0;
	}
	int result = net_network_poll(&net_ipv4_state, &net_udp_state,
	                              &net_tcp_state, &net_arp_state,
	                              e1000_receive_frame,
	                              e1000_send_frame);
	net_lock_release();
	if (result == 0) print("network poll: no frame\n");
	else if (result == 1) print("ICMP echo reply received\n");
	else if (result == 2) print("ICMP echo request replied\n");
	else if (result == NET_IPV4_POLL_ARP_REPLY)
		print("ARP reply received\n");
	else if (result == NET_IPV4_POLL_ARP_REQUEST_REPLIED)
		print("ARP request replied\n");
	else if (result == NET_NETWORK_POLL_UDP_REPLIED)
		print("UDP echo replied\n");
	else if (result == NET_NETWORK_POLL_UDP_QUEUED)
		print("UDP datagram queued\n");
	else if (result == NET_NETWORK_POLL_UDP_DROPPED)
		print("UDP datagram dropped\n");
	else if (result == NET_NETWORK_POLL_TCP_ESTABLISHED)
		print("TCP connection established\n");
	else if (result == NET_NETWORK_POLL_TCP_DATA)
		print("TCP data received\n");
	else if (result == NET_NETWORK_POLL_TCP_CLOSED)
		print("TCP connection closed\n");
	else {
		print("network poll: error ");
		print_int(result);
		print("\n");
	}
	return 0;
}

struct object *netstats(struct objectArray* args)
{
	if (!net_lock_acquire()) {
		print("network busy\n");
		return 0;
	}
	u32 arp_requests = net_arp_state.requests_sent;
	u32 arp_received = net_arp_state.replies_received;
	u32 arp_sent = net_arp_state.replies_sent;
	u32 arp_hits = net_arp_state.cache_hits;
	u32 arp_misses = net_arp_state.cache_misses;
	u32 arp_expired = net_arp_state.cache_expired;
	u32 icmp_sent = net_ipv4_state.echo_sent;
	u32 icmp_received = net_ipv4_state.echo_received;
	u32 icmp_replied = net_ipv4_state.echo_replied;
	u32 udp_sent = net_udp_state.sent;
	u32 udp_received = net_udp_state.received;
	u32 udp_replied = net_udp_state.replied;
	u32 udp_queued = net_udp_state.queued;
	u32 udp_drops = net_udp_state.drops;
	u32 dhcp_state = net_dhcp_state.state;
	u32 dhcp_retries = net_dhcp_state.retries;
	u32 dhcp_bound = net_dhcp_state.bound;
	u32 tcp_sent = net_tcp_state.sent;
	u32 tcp_received = net_tcp_state.received;
	u32 tcp_established = net_tcp_state.established;
	u32 tcp_closed = net_tcp_state.closed;
	u32 tcp_drops = net_tcp_state.drops;
	net_lock_release();
	print("ARP req:");
	print_int(arp_requests);
	print(" rx:");
	print_int(arp_received);
	print(" tx:");
	print_int(arp_sent);
	print(" hit:");
	print_int(arp_hits);
	print(" miss:");
	print_int(arp_misses);
	print(" expired:");
	print_int(arp_expired);
	print(" ICMP tx:");
	print_int(icmp_sent);
	print(" rx:");
	print_int(icmp_received);
	print(" reply:");
	print_int(icmp_replied);
	print(" UDP tx:");
	print_int(udp_sent);
	print(" rx:");
	print_int(udp_received);
	print(" reply:");
	print_int(udp_replied);
	print(" queued:");
	print_int(udp_queued);
	print(" drops:");
	print_int(udp_drops);
	print(" DHCP state:");
	print_int(dhcp_state);
	print(" retries:");
	print_int(dhcp_retries);
	print(" bound:");
	print_int(dhcp_bound);
	print(" TCP tx:");
	print_int(tcp_sent);
	print(" rx:");
	print_int(tcp_received);
	print(" established:");
	print_int(tcp_established);
	print(" closed:");
	print_int(tcp_closed);
	print(" drops:");
	print_int(tcp_drops);
	print("\n");
	return 0;
}

void net_process()
{
#ifdef NET_TCP_RUNTIME_TEST
	static u32 tcp_listener_started;
	static u32 tcp_test_payload_sent;
	static u32 tcp_test_info_printed;
#endif
	while (true) {
#ifdef NET_TCP_RUNTIME_TEST
		u32 tcp_test_print_info = 0;
#endif
		if (net_lock_acquire()) {
			net_arp_tick(&net_arp_state);
#ifdef NET_TCP_RUNTIME_TEST
			if (!tcp_listener_started) {
				net_tcp_listen(&net_tcp_state, 4001);
				tcp_listener_started = 1;
			}
#endif
			net_tcp_tick(&net_tcp_state, &net_arp_state, ticks,
			             e1000_send_frame);
			net_dhcp_tick(&net_dhcp_state, &net_arp_state,
			              &net_ipv4_state, &net_udp_state, ticks,
			              e1000_send_frame);
			for (u32 index = 0; index < 8; index++) {
				int result = net_network_poll(
					&net_ipv4_state, &net_udp_state, &net_tcp_state,
					&net_arp_state, e1000_receive_frame,
					e1000_send_frame);
				if (result == NET_NETWORK_POLL_UDP_QUEUED)
					net_dhcp_poll(&net_dhcp_state, &net_arp_state,
					              &net_ipv4_state, &net_udp_state,
					              ticks, 1000, e1000_send_frame);
				if (result <= 0) break;
			}
#ifdef NET_TCP_RUNTIME_TEST
			if (!tcp_test_payload_sent) {
				static const u8 first_payload[] = "kernel tcp one";
				static const u8 second_payload[] = "kernel tcp two";
				for (u32 index = 0; index < NET_TCP_CONNECTION_COUNT; index++) {
					if (!net_tcp_state.connections[index].used ||
					    net_tcp_state.connections[index].state !=
					    NET_TCP_STATE_ESTABLISHED) continue;
					if (net_tcp_send_data(&net_tcp_state, &net_arp_state,
							first_payload, sizeof(first_payload) - 1,
							e1000_send_frame) == 0 &&
					    net_tcp_send_data(&net_tcp_state, &net_arp_state,
							second_payload, sizeof(second_payload) - 1,
							e1000_send_frame) == 0)
						tcp_test_payload_sent = 1;
					break;
				}
			}
#endif
#ifdef NET_TCP_RUNTIME_TEST
			if (!tcp_test_info_printed) {
				for (u32 index = 0; index < NET_TCP_CONNECTION_COUNT; index++)
					if (net_tcp_state.connections[index].used &&
					    net_tcp_state.connections[index].state ==
					    NET_TCP_STATE_ESTABLISHED) {
						tcp_test_print_info = 1;
						break;
					}
			}
#endif
			net_lock_release();
#ifdef NET_TCP_RUNTIME_TEST
			if (tcp_test_print_info) {
				tcpinfo(0);
				tcp_test_info_printed = 1;
			}
#endif
		}
		asm volatile("hlt");
	}
}

struct object *cat(struct objectArray* args)
{
	if(args->count <1){
		KLOGE("use cat <path>\n");
		return 0;
	}
	
	void *buffer = read(args->objs[0].data,0);
	if(!buffer)return 0;
	print(buffer);

	free(buffer);

	return 0;
}
struct image*curimg=0;

struct object *img(struct objectArray* args)
{
	if(args->count <1){
		KLOGE("use img <path>\n");
		return 0;
	}
	if(curimg)free(curimg);
	curimg = read(args->objs[0].data,0);

	return 0;
}
struct object *hexdump_cmd(struct objectArray* args)
{
	if(args->count <1){
		KLOGE("use hexdump <path>\n");
		return 0;
	}
	int size = 0;

	void *buffer = read(args->objs[0].data,&size);
        if(!buffer)return 0;

	hexdump(buffer,size);

	free(buffer);

	return 0;
}

struct object *seldisk_cmd(struct objectArray *args)
{
	if(args->count <1){
                KLOGE("use seldisk <code>\n");
                return 0;
        }
	seldisk(args->objs[0].data);
	return 0;
}

struct object *ls(struct objectArray* args)
{
        struct disk *dsk =disks[current_diskid];
        BPB* bpb = read_first_sector(current_partstart,dsk);
        RootDir* rootdir=calculateRootDir(bpb);
        u8* root_buffer = readRootDir(current_partstart,dsk,rootdir,bpb);
        print_dir(root_buffer,bpb);
	
	free(bpb);free(rootdir);free(root_buffer);

        return 0;

}
struct object *random(struct objectArray* args)
{
	if(args->count<2)
	{
		KLOGE("use random <min> <max>\n");
		return 0;
	}

	u32 min = str2int(args->objs[0].data);	
	u32 max = str2int(args->objs[1].data);	

	u32 random = min+ (rand() % (max-min+1));
	print_int(random);
	print("\n");
	
	return 0;

}
struct object *dump_mem(struct objectArray*args)
{
	if(args->count<2){
                KLOGE("use dmpmem <start> <len>\n");
                return 0;
        }
        u32 start = str2int(args->objs[0].data);
        u32 len = str2int(args->objs[1].data);

	print_hex(start);
	print("\n");

        hexdump((void*)start,len);
        return 0;
}

struct object* uptime(struct objectArray *args)
{
	u32 all = ticks/1000;
	u32 sec = all%60;
	all/=60;
	u32 min = all%60;
	all/=60;
	u32 hour = all;

	if(hour>0){print_int(hour);print("h ");}
	if(min>0){print_int(min);print("m ");}
	print_int(sec);print("s\n");
	return 0;
}

struct object* touch(struct objectArray *args)
{
	if(args->count<1)
	{
		KLOGE("use touch <filename>");
		return 0;
	}
	write(args->objs[0].data,0,0);

	return 0;
}
struct object *wf(struct objectArray* args)
{
	if(args->count <2){
		KLOGE("use wf <path> <data>\n");
		return 0;
	}
	write(args->objs[0].data,args->objs[1].data,strlen(args->objs[1].data));

	return 0;
}

struct object *kill_cmd(struct objectArray *args)
{
	if(args->count<1){
		KLOGE("use kill <pid>\n");
		return 0;
	}
	
	int pid = str2int(args->objs[0].data);
	kill(pid);

	return 0;
}
struct object *sleep_cmd(struct objectArray *args)
{
	if(args->count<1){
		KLOGE("use sleep <millisecond>\n");
		return 0;
	}
	
	int m = str2int(args->objs[0].data);
	sleep(m);

	return 0;
}
struct object *ps(struct objectArray *args)
{

	print("PID | name\n");
	for(int i = 0;i<MAX_TASK_COUNT;i++)
	{
		if(!tasks[i])continue;
		print_int(i);
		print("     ");
		print(tasks[i]->name);
		print("\n");
	}

	return 0;	
}
struct object *reboot(struct objectArray *args)
{
	asm volatile("cli");
	u8 good = 0x02;
	while(good&0x02)good=inb(0x64);	

	outb(0x64,0xfe);

	return 0;	
}

struct object *poweroff(struct objectArray *args)
{
	int slptypea = 0,slptypeb = 0;

	if(args->count>=2){
		slptypea = str2int(args->objs[0].data);
		slptypeb = str2int(args->objs[1].data);
	}
	else if(args->count==1){
		slptypea = str2int(args->objs[0].data);
		slptypeb = str2int(args->objs[0].data);
	}

	asm volatile("cli");
	asm volatile("wbinvd");

	outw(fadt->PM1aControlBlock, (slptypea << 10) | (1 << 13));
	if (fadt->PM1bControlBlock != 0) {
		outw(fadt->PM1bControlBlock, (slptypeb << 10) | (1 << 13));
	}

	asm volatile("sti");
	reboot(0);

	return 0;
}


void emptyprocess()
{
	while(true)asm volatile("hlt");
}
s32 old_mouse_x=0,old_mouse_y=0;

void testdrawmouse()
{
	if (mouse_x == old_mouse_x && mouse_y == old_mouse_y) return;
    	
	for(int x = old_mouse_x-8;x<old_mouse_x+16;x++)for(int y = old_mouse_y-16;y<old_mouse_y+32;y++){
		put_pixel(x,y,0);
	}
	
	put_sym('^',mouse_x,mouse_y,0xFFFFFFFF,0);
	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;
}

void testdrawimage()
{
	if(curimg)drawimage(curimg,0,17);
}

extern void keyboard_isr_handler();

void main(char *cmdline){
	fpu_init();
#ifdef BIOSDISK
	init_bios();
#endif

	enableacpi();

	pic_remap();
	init_idt();
	set_idt_gate(33, (u32)keyboard_isr_handler);
	init_timer();
	init_cpu_exception();
	
	find_pci_devices();
	if (e1000_probe()) KLOGW("E1000 probe failed\n");
	else {
		KLOGI("E1000 initialized\n");
		e1000_print_info();
		u8 e1000_mac[6];
		if (e1000_get_mac(e1000_mac) == 0) {
			net_arp_init(&net_arp_state, e1000_mac, net_local_ip);
			net_ipv4_init(&net_ipv4_state, net_local_ip);
			net_tcp_init(&net_tcp_state);
			net_udp_init(&net_udp_state, NET_UDP_ECHO_PORT);
			net_dhcp_init(&net_dhcp_state, e1000_mac, 0x5A49524F);
		}
	}

#ifdef MOUSE
	mouse_init();
#endif

	register_function("reboot", reboot,0);
	register_function("poweroff", poweroff,0);
	register_function("random", random,"random value");
	register_function("img",img,"view image from file");
	register_function("wf",wf,"write to file");
	register_function("touch",touch,"create file");
	register_function("hexdump",hexdump_cmd,"print file in hexview");
	register_function("cat",cat,"print file to console");
	register_function("ls",ls,"print files in dirrectory");
	register_function("seldisk",seldisk_cmd,"select disk");
	register_function("dmpdsk",dump_disk,"lowlevel dump disk sectors");
	register_function("dmpmem",dump_mem,"lowlevel dump RAM");
	register_function("lspci",lspci,"print all pci device info");
	register_function("netinfo",netinfo,"print network device info");
	register_function("nettx",nettx,"send test Ethernet frame");
	register_function("netrx",netrx,"poll Ethernet RX ring");
	register_function("netarp",netarp,"send ARP request to gateway");
	register_function("netping",netping,"send ICMP echo to gateway");
	register_function("netudp",netudp,"send UDP datagram to gateway");
	register_function("dhcp",dhcp_cmd,"request IPv4 lease");
	register_function("udplisten",udplisten,"bind UDP receive port");
	register_function("udpunbind",udpunbind,"unbind UDP receive port");
	register_function("udprecv",udprecv,"receive queued UDP datagram");
	register_function("udpsend",udpsend,"send UDP text datagram");
	register_function("tcplisten",tcplisten,"bind TCP listen port");
	register_function("tcpconnect",tcpconnect,"connect TCP endpoint");
	register_function("tcpsend",tcpsend,"send TCP text");
	register_function("tcprecv",tcprecv,"receive TCP data");
	register_function("tcpclose",tcpclose,"close TCP connection");
	register_function("tcpinfo",tcpinfo,"print TCP state");
	register_function("netpoll",netpoll,"poll network frame");
	register_function("netstats",netstats,"print network counters");
	register_function("lsblk",lsblk,"print all disk");
	register_function("ps",ps,"list process");
	register_function("kill",kill_cmd,"kill process");
	register_function("uptime",uptime,0);
	register_function("sleep",sleep_cmd,"test sleep");
	register_function("date",date,"print date and time");
	register_function("fetch",screenfetch,"short system information");
	register_function("clear",clear,"clear screen");
	register_function("echo",echo,"print to console");
	register_function("help",help,0);
	
	KLOGI("system functions registered\n");	
	
	create_process((u32)windowsmanager,"windows manager",0);
	create_process((u32)start_shell,"shell",ega2fb);
	create_process((u32)net_process,"network",0);
#ifdef MOUSE
	create_process((u32)process_mouse,"mouse demo",testdrawmouse);
#endif
	create_process((u32)emptyprocess,"image demo",testdrawimage);
	create_process((u32)emptyprocess,"zhirGL demo",draw_some);

	is_interrupt_enabled = true;
	asm volatile ("sti");
}

#include "multiboot.h"
