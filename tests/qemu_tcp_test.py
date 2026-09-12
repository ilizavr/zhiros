#!/usr/bin/env python3
import socket
import struct
import sys
import time

from qemu_socket_test import (
    ETH_IPV4,
    GUEST_MAC,
    GUEST_IP,
    PEER_MAC,
    PEER_IP,
    ethernet,
    ipv4_packet,
    receive_frame,
    send_frame,
)

IP_TCP = 6
TCP_SYN = 0x002
TCP_ACK = 0x010
TCP_FIN = 0x001
TCP_PSH = 0x008
TCP_OPTION_EOL = 0
TCP_OPTION_NOP = 1
TCP_OPTION_MSS = 2
TCP_OPTION_WINDOW_SCALE = 3
TCP_OPTION_SACK_PERMITTED = 4
TCP_OPTION_SACK = 5


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


def tcp_options(frame):
    offset = 14 + ((frame[14] & 0x0F) * 4)
    tcp_offset = ((struct.unpack("!H", frame[offset + 12:offset + 14])[0] >> 12) * 4)
    options = frame[offset + 20:offset + tcp_offset]
    parsed = {}
    sack_blocks = []
    index = 0
    while index < len(options):
        kind = options[index]
        index += 1
        if kind == TCP_OPTION_EOL:
            break
        if kind == TCP_OPTION_NOP:
            continue
        if index >= len(options):
            raise RuntimeError("truncated TCP option")
        length = options[index]
        index += 1
        if length < 2 or index + length - 2 > len(options):
            raise RuntimeError("invalid TCP option length")
        value = options[index:index + length - 2]
        if kind == TCP_OPTION_MSS and length == 4:
            parsed["mss"] = struct.unpack("!H", value)[0]
        elif kind == TCP_OPTION_WINDOW_SCALE and length == 3:
            parsed["window_scale"] = value[0]
        elif kind == TCP_OPTION_SACK_PERMITTED and length == 2:
            parsed["sack_permitted"] = True
        elif kind == TCP_OPTION_SACK and length >= 10 and (length - 2) % 8 == 0:
            for block in range(0, len(value), 8):
                sack_blocks.append(struct.unpack("!II", value[block:block + 8]))
        index += length - 2
    parsed["sack"] = sack_blocks
    return parsed


def tcp_frame(flags, sequence, acknowledgement, payload=b"", options=b"",
              window=512):
    if len(options) % 4:
        raise ValueError("TCP options must be four-byte aligned")
    header_length = 20 + len(options)
    header = struct.pack(
        "!HHIIHHHH",
        9001,
        4001,
        sequence,
        acknowledgement,
        ((header_length // 4) << 12) | flags,
        window,
        0,
        0,
    ) + options
    pseudo = PEER_IP + GUEST_IP + struct.pack("!BBH", 0, IP_TCP,
                                               len(header) + len(payload))
    checksum_value = checksum(pseudo + header + payload)
    header = header[:16] + struct.pack("!H", checksum_value) + header[18:]
    return ethernet(
        GUEST_MAC,
        PEER_MAC,
        ETH_IPV4,
        ipv4_packet(IP_TCP, header + payload),
    ).ljust(60, b"\0")


def tcp_info(frame):
    offset = 14 + ((frame[14] & 0x0F) * 4)
    sequence, acknowledgement = struct.unpack("!II", frame[offset + 4:offset + 12])
    flags = struct.unpack("!H", frame[offset + 12:offset + 14])[0] & 0x1FF
    return sequence, acknowledgement, flags


def is_tcp(frame, flags):
    if len(frame) < 54 or struct.unpack("!H", frame[12:14])[0] != ETH_IPV4:
        return False
    if frame[23] != IP_TCP or frame[0:6] != PEER_MAC or frame[6:12] != GUEST_MAC:
        return False
    return tcp_info(frame)[2] & flags == flags


def is_guest_tcp(frame, flags):
    if len(frame) < 54 or struct.unpack("!H", frame[12:14])[0] != ETH_IPV4:
        return False
    if frame[23] != IP_TCP or frame[0:6] != PEER_MAC or frame[6:12] != GUEST_MAC:
        return False
    return tcp_info(frame)[2] & flags == flags


def tcp_payload(frame):
    ip_offset = 14
    ip_length = (frame[ip_offset] & 0x0F) * 4
    tcp_offset = ip_offset + ip_length
    tcp_length = ((struct.unpack("!H", frame[tcp_offset + 12:tcp_offset + 14])[0] >> 12) * 4)
    total_length = struct.unpack("!H", frame[ip_offset + 2:ip_offset + 4])[0]
    payload_start = tcp_offset + tcp_length
    payload_end = ip_offset + total_length
    return frame[payload_start:payload_end]


def receive_until(peer, predicate, label):
    deadline = time.time() + 4
    while time.time() < deadline:
        try:
            frame = receive_frame(peer, max(0.05, deadline - time.time()))
        except socket.timeout:
            continue
        if predicate(frame):
            print(label)
            return frame
    raise RuntimeError(label + " timeout")


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

    def expect_malformed_drop(label):
        deadline = time.time() + 0.5
        while time.time() < deadline:
            try:
                frame = receive_frame(peer, max(0.05, deadline - time.time()))
            except socket.timeout:
                print(label)
                return
            if is_tcp(frame, 0):
                raise RuntimeError(label + " unexpected TCP response")
        raise RuntimeError(label + " timeout")

    malformed_options = bytes((TCP_OPTION_MSS, 5, 1, 0))
    send_frame(peer, tcp_frame(TCP_SYN, 900, 0, options=malformed_options))
    expect_malformed_drop("socket TCP malformed options: OK")
    bad_checksum = bytearray(tcp_frame(TCP_SYN, 901, 0))
    bad_checksum[14 + 10] ^= 1
    send_frame(peer, bad_checksum)
    expect_malformed_drop("socket TCP malformed checksum: OK")
    send_frame(peer, tcp_frame(TCP_SYN | TCP_FIN, 902, 0))
    expect_malformed_drop("socket TCP malformed flags: OK")

    peer_sequence = 1000
    syn_options = bytes((
        TCP_OPTION_MSS, 4, 1, 0,
        TCP_OPTION_NOP, TCP_OPTION_WINDOW_SCALE, 3, 2,
        TCP_OPTION_SACK_PERMITTED, 2, TCP_OPTION_EOL, TCP_OPTION_EOL,
    ))
    send_frame(peer, tcp_frame(TCP_SYN, peer_sequence, 0, options=syn_options))
    syn_ack_seen = [False]

    def accept_second_syn_ack(frame):
        if not is_tcp(frame, TCP_SYN | TCP_ACK):
            return False
        if not syn_ack_seen[0]:
            syn_ack_seen[0] = True
            return False
        return True

    syn_ack = receive_until(peer, accept_second_syn_ack,
                            "socket TCP SYN retransmit: OK")
    guest_sequence, guest_ack, _ = tcp_info(syn_ack)
    negotiated = tcp_options(syn_ack)
    if negotiated.get("mss") is None or negotiated.get("window_scale") != 2 or \
            not negotiated.get("sack_permitted"):
        raise RuntimeError("TCP SYN-ACK options negotiation failed")
    print("socket TCP options: OK")

    send_frame(peer, tcp_frame(TCP_ACK, peer_sequence + 1, guest_sequence + 1,
                               window=512))
    print("socket TCP ACK: sent")

    first_guest_payload = b"kernel tcp one"
    second_guest_payload = b"kernel tcp two"
    first_guest_sequence = guest_sequence + 1
    second_guest_sequence = first_guest_sequence + len(first_guest_payload)
    def accept_second_guest_data(frame):
        if not is_guest_tcp(frame, TCP_ACK | TCP_PSH):
            return False
        sequence, _, _ = tcp_info(frame)
        if sequence == first_guest_sequence:
            return False
        return sequence == second_guest_sequence

    second_guest_data = receive_until(
        peer, accept_second_guest_data, "socket TCP SACK data: OK")
    if tcp_payload(second_guest_data) != second_guest_payload:
        raise RuntimeError("unexpected guest SACK payload")
    sack_options = bytes((TCP_OPTION_NOP, TCP_OPTION_NOP, TCP_OPTION_SACK, 10)) + \
        struct.pack("!II", second_guest_sequence,
                    second_guest_sequence + len(second_guest_payload))
    send_frame(peer, tcp_frame(TCP_ACK, peer_sequence + 1,
                               first_guest_sequence, options=sack_options,
                               window=512))
    retransmitted_first = receive_until(
        peer,
        lambda frame: is_guest_tcp(frame, TCP_ACK | TCP_PSH) and
        tcp_info(frame)[0] == first_guest_sequence,
        "socket TCP SACK recovery: OK",
    )
    if tcp_payload(retransmitted_first) != first_guest_payload:
        raise RuntimeError("unexpected SACK retransmission")
    guest_acknowledgement = second_guest_sequence + len(second_guest_payload)
    send_frame(peer, tcp_frame(TCP_ACK, peer_sequence + 1,
                               guest_acknowledgement, window=512))

    first_payload = b"hello"
    second_payload = b" tcp"
    receive_start = peer_sequence + 1
    send_frame(peer, tcp_frame(TCP_ACK | TCP_PSH,
                               receive_start + len(first_payload),
                               guest_acknowledgement, second_payload))
    out_of_order_ack = receive_until(
        peer,
        lambda frame: is_tcp(frame, TCP_ACK) and
        tcp_info(frame)[1] == receive_start,
        "socket TCP out-of-order ACK: OK",
    )
    sack = tcp_options(out_of_order_ack).get("sack", [])
    if (receive_start + len(first_payload),
            receive_start + len(first_payload) + len(second_payload)) not in sack:
        raise RuntimeError("TCP SACK block missing")
    print("socket TCP SACK: OK")

    send_frame(peer, tcp_frame(TCP_ACK | TCP_PSH, receive_start,
                               guest_acknowledgement, first_payload))
    receive_until(
        peer,
        lambda frame: is_tcp(frame, TCP_ACK) and
        tcp_info(frame)[1] == receive_start + len(first_payload) + len(second_payload),
        "socket TCP reordered data ACK: OK",
    )

    send_frame(peer, tcp_frame(TCP_ACK | TCP_PSH, receive_start,
                               guest_acknowledgement, first_payload))
    receive_until(
        peer,
        lambda frame: is_tcp(frame, TCP_ACK) and
        tcp_info(frame)[1] == receive_start + len(first_payload) + len(second_payload),
        "socket TCP duplicate data ACK: OK",
    )

    fin_sequence = receive_start + len(first_payload) + len(second_payload)
    send_frame(peer, tcp_frame(TCP_FIN | TCP_ACK, fin_sequence,
                               guest_acknowledgement))
    receive_until(peer, lambda frame: is_tcp(frame, TCP_ACK),
                  "socket TCP FIN ACK: OK")
    peer.close()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print("socket TCP test: FAIL", error)
        sys.exit(1)
