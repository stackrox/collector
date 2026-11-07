module NetworkOps

open NetworkTypes

module U8 = FStar.UInt8
module U16 = FStar.UInt16
module U64 = FStar.UInt64

// --- Localhost detection ---

let address_is_local (a:address) : bool =
  match a.family with
  | FamilyIPv4 ->
      U64.eq (U64.logand a.hi 0x00000000000000ffUL) 0x000000000000007fUL
  | FamilyIPv6 ->
      if not (U64.eq a.hi 0UL) then false
      else
        U64.eq a.lo 0x0100000000000000UL ||
        U64.eq (U64.logand a.lo 0x0000007fffff0000UL) 0x0000007fffff0000UL
  | FamilyUnknown -> false

// --- Private network checks (unrolled to avoid F* list in extracted code) ---

// Direct byte-mask checks matching C++ PrivateNetworks() on little-endian x86.
// The C++ stores IPv4 as raw bytes in the low 32 bits of hi.
// On LE, byte 0 of the IP is in the lowest byte of the uint64.
//
// 10.0.0.0/8:       first byte == 0x0a            mask=0xff  net=0x0a
// 100.64.0.0/10:    first 10 bits == 0x64,0x40    mask=0xc0ff net=0x4064
// 169.254.0.0/16:   first 2 bytes == 0xa9,0xfe    mask=0xffff net=0xfea9
// 172.16.0.0/12:    first 12 bits == 0xac,0x10    mask=0xf0ff net=0x10ac
// 192.168.0.0/16:   first 2 bytes == 0xc0,0xa8    mask=0xffff net=0xa8c0
inline_for_extraction
let is_private_ipv4 (a:address) : bool =
  U64.eq (U64.logand a.hi 0x00000000000000ffUL) 0x000000000000000aUL ||
  U64.eq (U64.logand a.hi 0x000000000000c0ffUL) 0x0000000000004064UL ||
  U64.eq (U64.logand a.hi 0x000000000000ffffUL) 0x000000000000fea9UL ||
  U64.eq (U64.logand a.hi 0x000000000000f0ffUL) 0x00000000000010acUL ||
  U64.eq (U64.logand a.hi 0x000000000000ffffUL) 0x000000000000a8c0UL

// IPv6 private ranges: fd00::/8 (ULA) + IPv4-mapped versions of all IPv4 private nets.
// IPv4-mapped addresses have hi=0, lo contains 0xffff marker + IPv4 bytes.
// On LE, lo byte layout: [0..1]=0x0000 [2..3]=0xffff [4..7]=IPv4 bytes
inline_for_extraction
let is_private_ipv6 (a:address) : bool =
  U64.eq (U64.logand a.hi 0x00000000000000ffUL) 0x00000000000000fdUL ||
  (U64.eq a.hi 0UL && (
    U64.eq (U64.logand a.lo 0x000000ffffff0000UL) 0x0000000affff0000UL ||
    U64.eq (U64.logand a.lo 0x0000c0ffffff0000UL) 0x00004064ffff0000UL ||
    U64.eq (U64.logand a.lo 0x0000ffffffff0000UL) 0x0000fea9ffff0000UL ||
    U64.eq (U64.logand a.lo 0x0000f0ffffff0000UL) 0x000010acffff0000UL ||
    U64.eq (U64.logand a.lo 0x0000ffffffff0000UL) 0x0000a8c0ffff0000UL
  ))

let address_is_public (a:address) : bool =
  match a.family with
  | FamilyIPv4 -> not (is_private_ipv4 a)
  | FamilyIPv6 -> not (is_private_ipv6 a)
  | FamilyUnknown -> false

// --- Ephemeral port detection ---
// All comparisons use machine U16 ops to avoid mathematical int

let is_ephemeral_port (port:U16.t) : U8.t =
  if U16.gte port 49152us then 4uy
  else if U16.gte port 32768us then 3uy
  else if U16.gte port 1025us && U16.lte port 5000us then 2uy
  else if U16.eq port 1024us then 1uy
  else 0uy

// --- Canonical external IP detection ---

let is_canonical_external (a:address) : bool =
  match a.family with
  | FamilyIPv4 ->
      U64.eq (U64.logand a.hi 0x00000000ffffffffUL) 0x00000000ffffffffUL
  | FamilyIPv6 ->
      U64.eq a.hi 0xffffffffffffffffUL && U64.eq a.lo 0xffffffffffffffffUL
  | FamilyUnknown -> false

// --- Proved properties (spec-only) ---

[@@noextract_to "krml"]
let lemma_unknown_not_public (a:address)
  : Lemma (requires a.family = FamilyUnknown)
          (ensures not (address_is_public a)) = ()

[@@noextract_to "krml"]
let lemma_unknown_not_local (a:address)
  : Lemma (requires a.family = FamilyUnknown)
          (ensures not (address_is_local a)) = ()

[@@noextract_to "krml"]
let lemma_ephemeral_iana_ge_linux (port:U16.t)
  : Lemma (requires U16.v port >= 49152)
          (ensures U8.v (is_ephemeral_port port) >= U8.v (is_ephemeral_port (U16.uint_to_t 32768))) = ()
