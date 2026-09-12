#!/usr/bin/env python3
import socket
import struct
import sys
import time

ETH_ARP = 0x0806
ETH_IPV4 = 0x0800
IP_ICMP = 1
IP_UDP = 17
GUEST_MAC = bytes.fromhex("525400123456")
PEER_MAC = bytes.fromhex("525400123457")
GUEST_IP = bytes([10, 0, 2, 15])
PEER_IP = bytes([10, 0, 2, 2])


def checksum(data):
    if len(data) & 1:
        data += b"\0"
    total = 0
    for index in range(0, len(data), 2):
        total += (data[index] << 8) | data[index + 1]
        total = (total & 0xFFFF) + (total >> 16)
    while total >> 16:
        total = (total & 0xFFFF) + (total >> 16)
    return (~total) & 0xFFFF


def ethernet(destination, source, ethertype, payload):
    return destination + source + struct.pack("!H", ethertype) + payload


def arp_request():
    payload = struct.pack(
        "!HHBBH6s4s6s4s",
        1,
        ETH_IPV4,
        6,
        4,
        1,
        PEER_MAC,
        PEER_IP,
        b"\0" * 6,
        GUEST_IP,
    )
    return ethernet(b"\xff" * 6, PEER_MAC, ETH_ARP, payload).ljust(60, b"\0")


def ipv4_packet(protocol, payload, source_ip=PEER_IP, destination_ip=GUEST_IP):
    header = struct.pack(
        "!BBHHHBBH4s4s",
        0x45,
        0,
        20 + len(payload),
        1,
        0,
        64,
        protocol,
        0,
        source_ip,
        destination_ip,
    )
    header = header[:10] + struct.pack("!H", checksum(header)) + header[12:]
    return header + payload


def icmp_request():
    body = struct.pack("!BBHHH", 8, 0, 0, 0x5A51, 1) + b"socket icmp"
    body = body[:2] + struct.pack("!H", checksum(body)) + body[4:]
    return ethernet(
        GUEST_MAC,
        PEER_MAC,
        ETH_IPV4,
        ipv4_packet(IP_ICMP, body),
    ).ljust(60, b"\0")


def udp_request(destination_port=7, payload=b"socket udp"):
    udp = struct.pack("!HHHH", 9000, destination_port, 8 + len(payload), 0) + payload
    pseudo = PEER_IP + GUEST_IP + struct.pack("!BBH", 0, IP_UDP, len(udp))
    udp = udp[:6] + struct.pack("!H", checksum(pseudo + udp)) + udp[8:]
    return ethernet(
        GUEST_MAC,
        PEER_MAC,
        ETH_IPV4,
        ipv4_packet(IP_UDP, udp),
    ).ljust(60, b"\0")


def read_exact(sock, length):
    result = bytearray()
    while len(result) < length:
        chunk = sock.recv(length - len(result))
        if not chunk:
            raise RuntimeError("socket peer closed")
        result.extend(chunk)
    return bytes(result)


def send_frame(sock, frame):
    sock.sendall(struct.pack("!I", len(frame)) + frame)


def receive_frame(sock, timeout=4):
    sock.settimeout(timeout)
    length = struct.unpack("!I", read_exact(sock, 4))[0]
    if length < 14 or length > 2048:
        raise RuntimeError("invalid frame length: %d" % length)
    return read_exact(sock, length)


def receive_until(sock, predicate, label):
    deadline = time.time() + 4
    while time.time() < deadline:
        try:
            frame = receive_frame(sock, max(0.05, deadline - time.time()))
        except socket.timeout:
            continue
        if predicate(frame):
            print(label)
            return frame
    raise RuntimeError(label + " timeout")


def is_arp_reply(frame):
    return (
        len(frame) >= 42
        and frame[0:6] == PEER_MAC
        and frame[6:12] == GUEST_MAC
        and struct.unpack("!H", frame[12:14])[0] == ETH_ARP
        and struct.unpack("!H", frame[20:22])[0] == 2
    )


def is_icmp_reply(frame):
    return (
        len(frame) >= 42
        and frame[0:6] == PEER_MAC
        and frame[6:12] == GUEST_MAC
        and struct.unpack("!H", frame[12:14])[0] == ETH_IPV4
        and frame[23] == IP_ICMP
        and frame[34] == 0
        and struct.unpack("!H", frame[38:40])[0] == 0x5A51
    )


def is_udp_reply(frame):
    return (
        len(frame) >= 42
        and frame[0:6] == PEER_MAC
        and frame[6:12] == GUEST_MAC
        and struct.unpack("!H", frame[12:14])[0] == ETH_IPV4
        and frame[23] == IP_UDP
        and struct.unpack("!H", frame[34:36])[0] == 7
        and struct.unpack("!H", frame[36:38])[0] == 9000
    )


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 12345
    peer = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    deadline = time.time() + 5
    while True:
        try:
            peer.connect(("127.0.0.1", port))
            break
        except ConnectionRefusedError:
            if time.time() >= deadline:
                raise
            time.sleep(0.05)
    send_frame(peer, arp_request())
    receive_until(peer, is_arp_reply, "socket ARP reply: OK")
    send_frame(peer, icmp_request())
    receive_until(peer, is_icmp_reply, "socket ICMP echo reply: OK")
    send_frame(peer, udp_request())
    receive_until(peer, is_udp_reply, "socket UDP echo reply: OK")
    peer.close()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print("socket network test: FAIL:", error)
        sys.exit(1)
