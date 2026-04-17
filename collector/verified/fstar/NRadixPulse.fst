module NRadixPulse
#lang-pulse

open Pulse.Lib.Pervasives

module Box = Pulse.Lib.Box
open Pulse.Lib.Box { box, (:=), (!) }

open NetworkTypes
open NRadixSpec

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
