module NetworkTypes

module U8 = FStar.UInt8
module U16 = FStar.UInt16
module U32 = FStar.UInt32
module U64 = FStar.UInt64

type address_family : eqtype =
  | FamilyUnknown : address_family
  | FamilyIPv4    : address_family
  | FamilyIPv6    : address_family

type address = {
  hi:     U64.t;
  lo:     U64.t;
  family: address_family;
}

let null_address : address = {
  hi = 0UL;
  lo = 0UL;
  family = FamilyUnknown;
}

let mk_ipv4 (hi:U64.t) : address = {
  hi = hi;
  lo = 0UL;
  family = FamilyIPv4;
}

let mk_ipv6 (hi lo:U64.t) : address = {
  hi = hi;
  lo = lo;
  family = FamilyIPv6;
}

let address_eq (a b:address) : bool =
  a.family = b.family && U64.eq a.hi b.hi && U64.eq a.lo b.lo

let address_is_null (a:address) : bool =
  U64.eq a.hi 0UL && U64.eq a.lo 0UL

// IPNet stores pre-masked address halves + prefix length
type ipnet = {
  addr:    address;
  mask_hi: U64.t;
  mask_lo: U64.t;
  prefix:  U8.t;
}

// Build mask by shifting ~0 right by (64 - bits) to get the high-bit mask.
// All comparisons use machine integers to avoid mathematical int in extraction.

inline_for_extraction
let make_mask_64 (bits:U8.t{U8.v bits >= 1 /\ U8.v bits <= 63}) : U64.t =
  let shift = U8.sub 64uy bits in
  U64.shift_right 0xffffffffffffffffUL (FStar.Int.Cast.uint8_to_uint32 shift)

let mk_ipnet (addr:address) (prefix:U8.t) : ipnet =
  let mask_hi =
    if U8.gte prefix 64uy then addr.hi
    else if U8.eq prefix 0uy then 0UL
    else U64.logand addr.hi (make_mask_64 prefix)
  in
  let mask_lo =
    if U8.gte prefix 128uy then addr.lo
    else if U8.lte prefix 64uy then 0UL
    else
      let lo_bits = U8.sub prefix 64uy in
      U64.logand addr.lo (make_mask_64 lo_bits)
  in
  { addr = addr; mask_hi = mask_hi; mask_lo = mask_lo; prefix = prefix }

let ipnet_contains (net:ipnet) (a:address) : bool =
  if net.addr.family <> a.family then false
  else if U8.eq net.prefix 0uy then true
  else if U8.gte net.prefix 128uy then
    U64.eq a.hi net.mask_hi && U64.eq a.lo net.mask_lo
  else if U8.gte net.prefix 64uy then
    if not (U64.eq a.hi net.mask_hi) then false
    else
      let lo_bits = U8.sub net.prefix 64uy in
      if U8.eq lo_bits 0uy then true
      else
        let lo_mask = make_mask_64 lo_bits in
        U64.eq (U64.logand a.lo lo_mask) net.mask_lo
  else
    let hi_mask = make_mask_64 net.prefix in
    U64.eq (U64.logand a.hi hi_mask) net.mask_hi

type l4proto : eqtype =
  | ProtoUnknown : l4proto
  | ProtoTCP     : l4proto
  | ProtoUDP     : l4proto
  | ProtoICMP    : l4proto

type endpoint = {
  net:  ipnet;
  port: U16.t;
}

let endpoint_is_null (ep:endpoint) : bool =
  U16.eq ep.port 0us && U8.eq ep.net.prefix 0uy && address_is_null ep.net.addr
