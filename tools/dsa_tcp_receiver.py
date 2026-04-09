#!/usr/bin/env python3
"""
Minimal TCP receiver for the DSA network stream.

Protocol:
  uint32 magic            = 0x4E415344  # "DSAN"
  uint32 version          = 1
  uint32 channel_count
  uint32 point_count_per_channel
  uint64 timestamp_ms
  uint32 sample_rate_sel
  uint32 sample_mode
  uint32 trigger_source
  uint32 external_trigger_edge
  uint32 clock_base
  float64[channel_count * point_count_per_channel] samples

All fields use little-endian byte order.
"""

from __future__ import annotations

import argparse
import socket
import struct
from typing import Tuple


HEADER_STRUCT = struct.Struct("<IIIIQIIIII")
EXPECTED_MAGIC = 0x4E415344
EXPECTED_VERSION = 1


def recv_exact(sock: socket.socket, size: int) -> bytes:
    chunks = []
    remaining = size
    while remaining > 0:
        chunk = sock.recv(remaining)
        if not chunk:
            raise ConnectionError("socket closed by peer")
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


def recv_frame(sock: socket.socket) -> Tuple[tuple, bytes]:
    header = recv_exact(sock, HEADER_STRUCT.size)
    values = HEADER_STRUCT.unpack(header)
    magic, version, channel_count, point_count, *_ = values
    if magic != EXPECTED_MAGIC:
        raise ValueError(f"unexpected magic: 0x{magic:08X}")
    if version != EXPECTED_VERSION:
        raise ValueError(f"unsupported version: {version}")
    payload_size = channel_count * point_count * 8
    payload = recv_exact(sock, payload_size)
    return values, payload


def main() -> None:
    parser = argparse.ArgumentParser(description="Receive DSA acquisition stream frames over TCP.")
    parser.add_argument("--host", default="0.0.0.0", help="Bind host, default: 0.0.0.0")
    parser.add_argument("--port", type=int, default=9000, help="Bind port, default: 9000")
    args = parser.parse_args()

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((args.host, args.port))
    server.listen(1)
    print(f"Listening on {args.host}:{args.port}")

    while True:
        conn, addr = server.accept()
        print(f"Client connected: {addr[0]}:{addr[1]}")
        with conn:
            frame_index = 0
            try:
                while True:
                    header, payload = recv_frame(conn)
                    (
                        _magic,
                        _version,
                        channel_count,
                        point_count,
                        timestamp_ms,
                        sample_rate_sel,
                        sample_mode,
                        trigger_source,
                        external_trigger_edge,
                        clock_base,
                    ) = header
                    frame_index += 1
                    print(
                        "frame=%d ts=%d channels=%d points=%d bytes=%d rate=%d mode=%d trigger=%d edge=%d clock=%d"
                        % (
                            frame_index,
                            timestamp_ms,
                            channel_count,
                            point_count,
                            len(payload),
                            sample_rate_sel,
                            sample_mode,
                            trigger_source,
                            external_trigger_edge,
                            clock_base,
                        )
                    )
            except Exception as exc:  # pragma: no cover - manual tool
                print(f"Connection ended: {exc}")


if __name__ == "__main__":
    main()
