module NRadixPulse
#lang-pulse

open Pulse.Lib.Pervasives

module Box = Pulse.Lib.Box
open Pulse.Lib.Box { box, (:=), (!) }

open NetworkTypes
open NRadixSpec

module U8 = FStar.UInt8
module U32 = FStar.UInt32
module U64 = FStar.UInt64

// Heap node type for the radix tree
noeq
type node_t = {
  has_value: bool;
  value:     ipnet;
  left:      tree_ptr;
  right:     tree_ptr;
}

and node_box = box node_t

// A nullable pointer to a node (None = leaf/null)
and tree_ptr = option node_box

// Find result type (extractable)
type pulse_find_result = {
  found: bool;
  net:   ipnet;
}

// Ghost invariant predicate: recursive slprop linking heap tree to spec tree
let rec is_tree ([@@@mkey] ct: tree_ptr) (ft: NRadixSpec.nradix_node)
  : Tot slprop (decreases ft)
  = match ft with
    | NRLeaf -> pure (ct == None)
    | NRNode has_value value left right ->
      exists* (p: node_box) (lct rct: tree_ptr).
        pure (ct == Some p) **
        (p |-> { has_value; value; left = lct; right = rct }) **
        is_tree lct left **
        is_tree rct right

// Tree handle type
noeq
type nradix_tree = {
  root: tree_ptr;
}

// Convenience: null tree pointer
let null_tree_ptr : tree_ptr = None #node_box

// Helper: case analysis on the is_tree predicate
[@@no_mkeys]
let is_tree_cases (x: option node_box) (ft: NRadixSpec.nradix_node)
  = match x with
    | None -> pure (ft == NRLeaf)
    | Some v ->
      exists* (n: node_t) (ltree rtree: NRadixSpec.nradix_node).
        (v |-> n) **
        pure (ft == NRNode n.has_value n.value ltree rtree) **
        is_tree n.left ltree **
        is_tree n.right rtree

ghost
fn elim_is_tree_leaf (x: tree_ptr)
  requires is_tree x NRLeaf
  ensures pure (x == None)
{
  unfold (is_tree x NRLeaf)
}

ghost
fn intro_is_tree_leaf (x: tree_ptr)
  requires pure (x == None)
  ensures is_tree x NRLeaf
{
  fold (is_tree x NRLeaf)
}

ghost
fn elim_is_tree_node (ct: tree_ptr)
                     (has_value: bool) (value: ipnet)
                     (ltree rtree: NRadixSpec.nradix_node)
  requires is_tree ct (NRNode has_value value ltree rtree)
  ensures (
    exists* (p: node_box) (lct rct: tree_ptr).
      pure (ct == Some p) **
      (p |-> { has_value; value; left = lct; right = rct }) **
      is_tree lct ltree **
      is_tree rct rtree
  )
{
  unfold is_tree
}

ghost
fn intro_is_tree_node (ct: tree_ptr) (v: node_box)
                      (#node: node_t)
                      (#ltree #rtree: NRadixSpec.nradix_node)
  requires
    (v |-> node) **
    is_tree node.left ltree **
    is_tree node.right rtree **
    pure (ct == Some v)
  ensures
    is_tree ct (NRNode node.has_value node.value ltree rtree)
{
  fold (is_tree ct (NRNode node.has_value node.value ltree rtree))
}

ghost
fn cases_of_is_tree (x: tree_ptr) (ft: NRadixSpec.nradix_node)
  requires is_tree x ft
  ensures  is_tree_cases x ft
{
  match ft {
    NRLeaf -> {
      unfold (is_tree x NRLeaf);
      fold (is_tree_cases None ft);
      rewrite is_tree_cases None ft as is_tree_cases x ft;
    }
    NRNode has_value value ltree rtree -> {
      unfold (is_tree x (NRNode has_value value ltree rtree));
      with p lct rct. _;
      with n. assert p |-> n;
      with l'. rewrite is_tree lct l' as is_tree n.left l';
      with r'. rewrite is_tree rct r' as is_tree n.right r';
      fold (is_tree_cases (Some p) ft);
      rewrite (is_tree_cases (Some p) ft) as is_tree_cases x ft;
    }
  }
}

ghost
fn is_tree_case_none (x: tree_ptr) (#ft: NRadixSpec.nradix_node)
  preserves is_tree x ft
  requires pure (x == None)
  ensures pure (ft == NRLeaf)
{
  rewrite each x as None;
  cases_of_is_tree None ft;
  unfold is_tree_cases;
  intro_is_tree_leaf x;
  ()
}

ghost
fn is_tree_case_some (x: tree_ptr) (v: node_box) (#ft: NRadixSpec.nradix_node)
  requires is_tree x ft
  requires pure (x == Some v)
  ensures exists* (node: node_t) (ltree rtree: NRadixSpec.nradix_node).
    (v |-> node) **
    is_tree node.left ltree **
    is_tree node.right rtree **
    pure (ft == NRNode node.has_value node.value ltree rtree)
{
  rewrite each x as Some v;
  cases_of_is_tree (Some v) ft;
  unfold is_tree_cases;
}

// ============================================================
// create: allocate an empty tree
// ============================================================

fn create ()
  requires emp
  returns t: nradix_tree
  ensures is_tree t.root nradix_empty
{
  let root = Box.alloc ({ has_value = false; value = null_ipnet; left = null_tree_ptr; right = null_tree_ptr });
  intro_is_tree_leaf null_tree_ptr;
  intro_is_tree_leaf null_tree_ptr;
  intro_is_tree_node (Some root) root;
  ({ root = Some root })
}

// ============================================================
// destroy: recursively free all nodes in the tree
// ============================================================

fn rec destroy_subtree (ct: tree_ptr) (#ft: erased NRadixSpec.nradix_node)
  requires is_tree ct ft
  ensures emp
  decreases ft
{
  cases_of_is_tree ct ft;
  match ct {
    None -> {
      unfold is_tree_cases;
      ()
    }
    Some p -> {
      unfold is_tree_cases;
      with node ltree rtree. _;
      let n = !p;
      destroy_subtree n.left #ltree;
      destroy_subtree n.right #rtree;
      Box.free p;
    }
  }
}

fn destroy (t: nradix_tree) (#ft: erased NRadixSpec.nradix_node)
  requires is_tree t.root ft
  ensures emp
{
  destroy_subtree t.root;
}

// ============================================================
// is_empty: check whether the tree contains no networks
// ============================================================

ghost
fn is_tree_none_iff_leaf (x: tree_ptr) (#ft: NRadixSpec.nradix_node)
  requires is_tree x ft
  ensures is_tree x ft ** pure (None? x == NRLeaf? ft)
{
  cases_of_is_tree x ft;
  match x {
    None -> {
      unfold is_tree_cases;
      intro_is_tree_leaf x;
    }
    Some p -> {
      unfold is_tree_cases;
      with node ltree rtree. _;
      intro_is_tree_node x p;
    }
  }
}

fn is_empty (t: nradix_tree) (#ft: erased NRadixSpec.nradix_node)
  requires is_tree t.root ft
  returns b: bool
  ensures is_tree t.root ft ** pure (b == NRadixSpec.is_empty (reveal ft))
{
  cases_of_is_tree t.root ft;
  match t.root {
    None -> {
      unfold is_tree_cases;
      intro_is_tree_leaf t.root;
      true
    }
    Some p -> {
      unfold is_tree_cases;
      with node ltree rtree. _;
      let n = !p;
      is_tree_none_iff_leaf n.left #ltree;
      is_tree_none_iff_leaf n.right #rtree;
      intro_is_tree_node t.root p;
      (not n.has_value && None? n.left && None? n.right)
    }
  }
}

// ============================================================
// find: read-only tree traversal returning the most specific
//       containing network (proved equal to NRadixSpec.find_aux)
// ============================================================

fn rec find_walk (ct: tree_ptr) (#ft: erased NRadixSpec.nradix_node)
                 (host_hi host_lo mask_hi mask_lo: U64.t)
                 (remaining: U8.t{U8.v remaining <= 128})
                 (best: NRadixSpec.find_result)
  requires is_tree ct ft
  returns result: NRadixSpec.find_result
  ensures is_tree ct ft **
          pure (result == NRadixSpec.find_aux (reveal ft) host_hi host_lo mask_hi mask_lo (U8.v remaining) best)
  decreases ft
{
  cases_of_is_tree ct ft;
  match ct {
    None -> {
      unfold is_tree_cases;
      intro_is_tree_leaf ct;
      best
    }
    Some p -> {
      unfold is_tree_cases;
      with node ltree rtree. _;
      let n = !p;
      let best' : NRadixSpec.find_result =
        (if n.has_value
         then ({ NRadixSpec.found = true; NRadixSpec.net = n.value })
         else best);
      if (U8.eq remaining 0uy) {
        intro_is_tree_node ct p;
        best'
      } else {
        let remaining' : U8.t = U8.sub remaining 1uy;
        let bit_pos : U8.t = U8.sub 128uy remaining;
        let bp : U32.t = FStar.Int.Cast.uint8_to_uint32 bit_pos;
        if not (NRadixSpec.is_in_mask bp mask_hi mask_lo) {
          intro_is_tree_node ct p;
          best'
        } else {
          let go_right = NRadixSpec.get_direction host_hi host_lo bp;
          if go_right {
            let result = find_walk n.right #rtree host_hi host_lo mask_hi mask_lo remaining' best';
            intro_is_tree_node ct p;
            result
          } else {
            let result = find_walk n.left #ltree host_hi host_lo mask_hi mask_lo remaining' best';
            intro_is_tree_node ct p;
            result
          }
        }
      }
    }
  }
}

fn find (t: nradix_tree) (#ft: erased NRadixSpec.nradix_node) (net: ipnet)
  requires is_tree t.root ft
  returns result: NRadixSpec.find_result
  ensures is_tree t.root ft **
          pure (result == NRadixSpec.find (reveal ft) net)
{
  if (U8.eq net.prefix 0uy) {
    NRadixSpec.no_result
  } else {
    let host_hi = NRadixSpec.bswap64 net.addr.hi;
    let host_lo = NRadixSpec.bswap64 net.addr.lo;
    let mask_hi = NRadixSpec.net_mask_hi net.prefix;
    let mask_lo = NRadixSpec.net_mask_lo net.prefix;
    find_walk t.root host_hi host_lo mask_hi mask_lo 128uy NRadixSpec.no_result
  }
}

fn find_addr (t: nradix_tree) (#ft: erased NRadixSpec.nradix_node) (a: address)
  requires is_tree t.root ft
  returns result: NRadixSpec.find_result
  ensures is_tree t.root ft **
          pure (result == NRadixSpec.find_addr (reveal ft) a)
{
  let net = mk_ipnet a (if a.family = FamilyIPv4 then 32uy else 128uy);
  find t net
}

// ============================================================
// insert: mutating tree traversal that adds a network
//         (proved equal to NRadixSpec.insert_aux / insert)
// ============================================================

// place_value on a None (NRLeaf): allocate a new node
fn place_value_leaf (net: ipnet)
  requires emp
  returns new_ct: tree_ptr
  ensures is_tree new_ct (NRNode true net NRLeaf NRLeaf)
{
  let b = Box.alloc ({ has_value = true; value = net; left = null_tree_ptr; right = null_tree_ptr });
  intro_is_tree_leaf null_tree_ptr;
  intro_is_tree_leaf null_tree_ptr;
  intro_is_tree_node (Some b) b;
  (Some b)
}

// place_value on a Some p (NRNode): set value if not already set
fn place_value_node (ct: tree_ptr) (p: node_box) (net: ipnet)
                    (#node: erased node_t)
                    (#ltree #rtree: erased NRadixSpec.nradix_node)
  requires
    (p |-> reveal node) **
    is_tree (reveal node).left ltree **
    is_tree (reveal node).right rtree **
    pure (ct == Some p)
  returns new_ct: tree_ptr
  ensures is_tree new_ct
    (if (reveal node).has_value
     then NRNode (reveal node).has_value (reveal node).value (reveal ltree) (reveal rtree)
     else NRNode true net (reveal ltree) (reveal rtree))
{
  let n = !p;
  if n.has_value {
    intro_is_tree_node ct p;
    ct
  } else {
    p := { n with has_value = true; value = net };
    intro_is_tree_node ct p;
    ct
  }
}

fn rec insert_walk (ct: tree_ptr) (#ft: erased NRadixSpec.nradix_node)
                   (net: ipnet)
                   (host_hi host_lo mask_hi mask_lo: U64.t)
                   (remaining: U8.t{U8.v remaining <= 128})
  requires is_tree ct ft
  returns new_ct: tree_ptr
  ensures is_tree new_ct (NRadixSpec.insert_aux (reveal ft) net host_hi host_lo mask_hi mask_lo (U8.v remaining))
  decreases (U8.v remaining)
{
  cases_of_is_tree ct ft;
  match ct {
    None -> {
      // ct is None, ft is NRLeaf
      unfold is_tree_cases;
      // place_value on NRLeaf:
      //   insert_aux NRLeaf ... remaining =
      //     if remaining = 0 then place_value NRLeaf
      //     else if not (is_in_mask ...) then place_value NRLeaf
      //     else NRNode false null_ipnet (recurse ...) NRLeaf  (or vice versa)
      if (U8.eq remaining 0uy) {
        // remaining = 0: place_value NRLeaf = NRNode true net NRLeaf NRLeaf
        place_value_leaf net
      } else {
        let remaining' : U8.t = U8.sub remaining 1uy;
        let bit_pos : U8.t = U8.sub 128uy remaining;
        let bp : U32.t = FStar.Int.Cast.uint8_to_uint32 bit_pos;
        if not (NRadixSpec.is_in_mask bp mask_hi mask_lo) {
          // mask exhausted: place_value NRLeaf
          place_value_leaf net
        } else {
          let go_right = NRadixSpec.get_direction host_hi host_lo bp;
          if go_right {
            // Recurse into right child (which is NRLeaf)
            intro_is_tree_leaf null_tree_ptr;
            let new_right = insert_walk null_tree_ptr #NRLeaf net host_hi host_lo mask_hi mask_lo remaining';
            // Build: NRNode false null_ipnet NRLeaf new_right
            intro_is_tree_leaf null_tree_ptr;
            let b = Box.alloc ({ has_value = false; value = NRadixSpec.null_ipnet; left = null_tree_ptr; right = new_right });
            intro_is_tree_node (Some b) b;
            (Some b)
          } else {
            // Recurse into left child (which is NRLeaf)
            intro_is_tree_leaf null_tree_ptr;
            let new_left = insert_walk null_tree_ptr #NRLeaf net host_hi host_lo mask_hi mask_lo remaining';
            // Build: NRNode false null_ipnet new_left NRLeaf
            intro_is_tree_leaf null_tree_ptr;
            let b = Box.alloc ({ has_value = false; value = NRadixSpec.null_ipnet; left = new_left; right = null_tree_ptr });
            intro_is_tree_node (Some b) b;
            (Some b)
          }
        }
      }
    }
    Some p -> {
      unfold is_tree_cases;
      with node ltree rtree. _;
      let n = !p;
      if (U8.eq remaining 0uy) {
        // remaining = 0: place_value (NRNode ...)
        if n.has_value {
          intro_is_tree_node ct p;
          ct
        } else {
          p := { n with has_value = true; value = net };
          intro_is_tree_node ct p;
          ct
        }
      } else {
        let remaining' : U8.t = U8.sub remaining 1uy;
        let bit_pos : U8.t = U8.sub 128uy remaining;
        let bp : U32.t = FStar.Int.Cast.uint8_to_uint32 bit_pos;
        if not (NRadixSpec.is_in_mask bp mask_hi mask_lo) {
          // mask exhausted: place_value
          if n.has_value {
            intro_is_tree_node ct p;
            ct
          } else {
            p := { n with has_value = true; value = net };
            intro_is_tree_node ct p;
            ct
          }
        } else {
          let go_right = NRadixSpec.get_direction host_hi host_lo bp;
          if go_right {
            let new_right = insert_walk n.right #rtree net host_hi host_lo mask_hi mask_lo remaining';
            p := { n with right = new_right };
            intro_is_tree_node ct p;
            ct
          } else {
            let new_left = insert_walk n.left #ltree net host_hi host_lo mask_hi mask_lo remaining';
            p := { n with left = new_left };
            intro_is_tree_node ct p;
            ct
          }
        }
      }
    }
  }
}

fn insert (t: nradix_tree) (#ft: erased NRadixSpec.nradix_node) (net: ipnet)
  requires is_tree t.root ft
  returns new_t: nradix_tree
  ensures is_tree new_t.root (NRadixSpec.insert (reveal ft) net)
{
  if (U8.eq net.prefix 0uy) {
    t
  } else if (U8.gt net.prefix 128uy) {
    t
  } else {
    let host_hi = NRadixSpec.bswap64 net.addr.hi;
    let host_lo = NRadixSpec.bswap64 net.addr.lo;
    let mask_hi = NRadixSpec.net_mask_hi net.prefix;
    let mask_lo = NRadixSpec.net_mask_lo net.prefix;
    let new_root = insert_walk t.root net host_hi host_lo mask_hi mask_lo 128uy;
    ({ root = new_root })
  }
}
