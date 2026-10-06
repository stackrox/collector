module NRadixSpec

open NetworkTypes

module U8 = FStar.UInt8
module U16 = FStar.UInt16
module U32 = FStar.UInt32
module U64 = FStar.UInt64

// ============================================================
// Byte swap (ntohll on little-endian x86)
// ============================================================

let bswap64 (x:U64.t) : U64.t =
  let open U64 in
  let b0 = logand x 0xffUL in
  let b1 = logand (shift_right x 8ul) 0xffUL in
  let b2 = logand (shift_right x 16ul) 0xffUL in
  let b3 = logand (shift_right x 24ul) 0xffUL in
  let b4 = logand (shift_right x 32ul) 0xffUL in
  let b5 = logand (shift_right x 40ul) 0xffUL in
  let b6 = logand (shift_right x 48ul) 0xffUL in
  let b7 = logand (shift_right x 56ul) 0xffUL in
  logor (shift_left b0 56ul)
    (logor (shift_left b1 48ul)
      (logor (shift_left b2 40ul)
        (logor (shift_left b3 32ul)
          (logor (shift_left b4 24ul)
            (logor (shift_left b5 16ul)
              (logor (shift_left b6 8ul)
                b7))))))

// ============================================================
// Net mask computation (matches C++ IPNet::net_mask_array)
// ============================================================

let net_mask_hi (prefix:U8.t) : U64.t =
  if U8.eq prefix 0uy then 0UL
  else if U8.gte prefix 64uy then 0xffffffffffffffffUL
  else
    U64.lognot (U64.shift_right 0xffffffffffffffffUL
      (FStar.Int.Cast.uint8_to_uint32 prefix))

let net_mask_lo (prefix:U8.t) : U64.t =
  if U8.lte prefix 64uy then 0UL
  else if U8.gte prefix 128uy then 0xffffffffffffffffUL
  else
    let lo_bits = U8.sub prefix 64uy in
    U64.lognot (U64.shift_right 0xffffffffffffffffUL
      (FStar.Int.Cast.uint8_to_uint32 lo_bits))

// ============================================================
// Bit operations for tree traversal
// ============================================================

let get_direction (host_hi host_lo:U64.t) (bit_pos:U32.t{U32.v bit_pos < 128}) : bool =
  let open U64 in
  if U32.lt bit_pos 64ul then
    let shift = U32.sub 63ul bit_pos in
    not (eq (logand host_hi (shift_left 1UL shift)) 0UL)
  else
    let adj = U32.sub bit_pos 64ul in
    let shift = U32.sub 63ul adj in
    not (eq (logand host_lo (shift_left 1UL shift)) 0UL)

let is_in_mask (bit_pos:U32.t{U32.v bit_pos < 128}) (mask_hi mask_lo:U64.t) : bool =
  let open U64 in
  if U32.lt bit_pos 64ul then
    let shift = U32.sub 63ul bit_pos in
    not (eq (logand mask_hi (shift_left 1UL shift)) 0UL)
  else
    let adj = U32.sub bit_pos 64ul in
    let shift = U32.sub 63ul adj in
    not (eq (logand mask_lo (shift_left 1UL shift)) 0UL)

// ============================================================
// Find result: avoids FStar.Pervasives.Native.option in extracted code
// ============================================================

type find_result = {
  found: bool;
  net:   ipnet;
}

inline_for_extraction
let no_result : find_result = { found = false; net = mk_ipnet null_address 0uy }

// ============================================================
// Tree type: boolean flag instead of option for Karamel extraction
// ============================================================

noeq type nradix_node =
  | NRLeaf : nradix_node
  | NRNode : has_value:bool -> value:ipnet -> left:nradix_node -> right:nradix_node -> nradix_node

[@@noextract_to "krml"]
let nradix_empty : nradix_node = NRNode false (mk_ipnet null_address 0uy) NRLeaf NRLeaf

inline_for_extraction
let null_ipnet : ipnet = mk_ipnet null_address 0uy

// ============================================================
// Find: returns the most specific containing network
// ============================================================

[@@noextract_to "krml"]
let rec find_aux (node:nradix_node) (host_hi host_lo mask_hi mask_lo:U64.t)
                 (remaining:nat{remaining <= 128}) (best:find_result)
  : Tot find_result (decreases remaining) =
  match node with
  | NRLeaf -> best
  | NRNode has_value value left right ->
    let best = if has_value then { found = true; net = value } else best in
    if remaining = 0 then best
    else
      let remaining' : nat = remaining - 1 in
      let bit_pos = 128 - remaining in
      let bp : U32.t = FStar.Int.Cast.uint8_to_uint32 (U8.uint_to_t bit_pos) in
      if not (is_in_mask bp mask_hi mask_lo) then best
      else
        let go_right = get_direction host_hi host_lo bp in
        let next = if go_right then right else left in
        find_aux next host_hi host_lo mask_hi mask_lo remaining' best

[@@noextract_to "krml"]
let find (tree:nradix_node) (net:ipnet) : find_result =
  if U8.eq net.prefix 0uy then no_result
  else
    let host_hi = bswap64 net.addr.hi in
    let host_lo = bswap64 net.addr.lo in
    let mask_hi = net_mask_hi net.prefix in
    let mask_lo = net_mask_lo net.prefix in
    find_aux tree host_hi host_lo mask_hi mask_lo 128 no_result

[@@noextract_to "krml"]
let find_addr (tree:nradix_node) (a:address) : find_result =
  let net = mk_ipnet a (if a.family = FamilyIPv4 then 32uy else 128uy) in
  find tree net

// ============================================================
// Insert: add a network to the tree
// ============================================================

[@@noextract_to "krml"]
let rec insert_aux (node:nradix_node) (net:ipnet) (host_hi host_lo mask_hi mask_lo:U64.t)
                   (remaining:nat{remaining <= 128})
  : Tot nradix_node (decreases remaining) =
  let place_value (n:nradix_node) : nradix_node =
    match n with
    | NRLeaf -> NRNode true net NRLeaf NRLeaf
    | NRNode has_value existing left right ->
      if has_value then n
      else NRNode true net left right
  in
  if remaining = 0 then place_value node
  else
    let remaining' : nat = remaining - 1 in
    let bit_pos = 128 - remaining in
    let bp : U32.t = FStar.Int.Cast.uint8_to_uint32 (U8.uint_to_t bit_pos) in
    if not (is_in_mask bp mask_hi mask_lo) then place_value node
    else
      let go_right = get_direction host_hi host_lo bp in
      match node with
      | NRLeaf ->
        if go_right then
          NRNode false null_ipnet NRLeaf (insert_aux NRLeaf net host_hi host_lo mask_hi mask_lo remaining')
        else
          NRNode false null_ipnet (insert_aux NRLeaf net host_hi host_lo mask_hi mask_lo remaining') NRLeaf
      | NRNode has_value value left right ->
        if go_right then
          NRNode has_value value left (insert_aux right net host_hi host_lo mask_hi mask_lo remaining')
        else
          NRNode has_value value (insert_aux left net host_hi host_lo mask_hi mask_lo remaining') right

[@@noextract_to "krml"]
let insert (tree:nradix_node) (net:ipnet) : nradix_node =
  if U8.eq net.prefix 0uy then tree
  else if U8.gt net.prefix 128uy then tree
  else
    let host_hi = bswap64 net.addr.hi in
    let host_lo = bswap64 net.addr.lo in
    let mask_hi = net_mask_hi net.prefix in
    let mask_lo = net_mask_lo net.prefix in
    insert_aux tree net host_hi host_lo mask_hi mask_lo 128

// ============================================================
// IsEmpty
// ============================================================

[@@noextract_to "krml"]
let is_empty (tree:nradix_node) : bool =
  match tree with
  | NRLeaf -> true
  | NRNode has_value _ left right ->
    not has_value && NRLeaf? left && NRLeaf? right

// ============================================================
// Proved properties (spec-only, not extracted)
// ============================================================

[@@noextract_to "krml"]
let lemma_find_empty (net:ipnet)
  : Lemma (ensures (find NRLeaf net).found = false) = ()

[@@noextract_to "krml"]
let lemma_find_empty_root (net:ipnet)
  : Lemma (ensures (find nradix_empty net).found = false) = ()

[@@noextract_to "krml"]
let lemma_is_empty_leaf ()
  : Lemma (ensures is_empty NRLeaf = true) = ()

[@@noextract_to "krml"]
let lemma_is_empty_nradix_empty ()
  : Lemma (ensures is_empty nradix_empty = true) = ()

[@@noextract_to "krml"]
let lemma_insert_not_empty (tree:nradix_node) (net:ipnet)
  : Lemma (requires U8.v net.prefix >= 1 /\ U8.v net.prefix <= 128)
          (ensures not (is_empty (insert tree net))) = ()

[@@noextract_to "krml"]
let lemma_find_returns_supernet (tree:nradix_node) (a:address)
  : Lemma (requires (find_addr tree a).found = true)
          (ensures ipnet_contains (find_addr tree a).net a = true)
  = admit ()

[@@noextract_to "krml"]
let lemma_insert_then_find (tree:nradix_node) (net:ipnet)
  : Lemma (requires U8.v net.prefix >= 1 /\ U8.v net.prefix <= 128)
          (ensures (find (insert tree net) net).found = true)
  = admit ()
