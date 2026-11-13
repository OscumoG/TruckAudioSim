// SPDX-License-Identifier: MIT
// Proposed 0.12.0 user/kernel interface contract (NOT an installed driver).
// Portable definitions; the future WDK handler MUST enforce device ACLs.
#ifndef TAS_IOCTL_CONTRACT_H
#define TAS_IOCTL_CONTRACT_H
#include <stdint.h>
#define TAS_IOCTL_GET_PACKET 0x00226000u
#define TAS_IOCTL_GET_STATUS 0x00226004u
#define TAS_IOCTL_STATUS_BYTES 40u
// CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS)
// A read-only, little-endian, fixed-size capabilities handshake:
// 0: magic TAST (u32 0x54534154), 4: version (u16 = 1), 6: bytes (u16 = 40)
// 8: sampleRate (u32 = 48000), 12: channelMask (u32)
// 16: channels (u16), 18: sampleFormat (u16 = 1, float32)
// 20: sourceToken (u32), 24: sessionGeneration (u64)
// 32: pendingPackets (u32), 36: droppedPacketsSaturated (u32)
// Must use same authenticated reader handle as GET_PACKET. Never returns PCM.
// CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)
// Request: none. Successful response: one complete TAS1 PCM packet with
// exact bytes returned (<= TAS_FIFO_MAX_PACKET_BYTES), no kernel pointers.
// Empty: ERROR_NO_MORE_ITEMS. Buffer too small: ERROR_INSUFFICIENT_BUFFER.
// Consumer gets *only* authenticated-by-device-handle PCM data; the source
// token is a ROUTING ID, NOT a security credential.
#endif
