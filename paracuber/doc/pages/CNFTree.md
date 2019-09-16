CNF Tree {#CNFTree}
===================

The CNFTree concept is the core concept behind coordinating the parallel cubing over multiple
compute nodes. The formula is divided into multiple paths to take
while cubing, each path is made unique using the sequence of choices combined
with the depth of the tree required to arrive at the desired location. A `0` means go to the
left, a `1` to the right. The following graphic visualises such a tree.

@dotfile cnftree-schematic.dot

The whole tree will never be completely materialised on all compute nodes, instead
each compute node has its own version of the tree in the form of a disjoint sub-tree. The tree is
represented by the paracuber::CNFTree class, which also implements the visitor to walk over
any given decision procedure defined by a paracuber::CNFTree::Path variable.

Root Formula
------------

The root formula is the one with its `previous` field being equal to `0`. This
means `depth=0` and `path=0`. This is the only node in the tree that contains
the complete original CNF formula. All descendants only reference this root.

Cubes
-----

Cubes branch off of the root formula and go down into the tree. No single cube is manifested directly - instead,
cubes are calculated while traversing the tree. They are then applied to the SAT solver by stepping through the tree.

Applying Cubes to a solver inside a paracuber::CaDiCaLTask
----------------------------------------------------------

To apply a cube to a solver, the previous solver is copied and the new cube is applied. This
prevents the solver from re-parsing the entire tree and only changes need to be applied. Every
branch can also be parallelised. This step happens in paracuber::CaDiCaLTask::readCNF.

A cube can only be applied to a solver that already has all previous
cubes applied to it, which means to apply a new cube to an existing solver instance, the depths
and paths must be matched between the currently inserted CNF formula in the solver and the new cube.

The most efficient way to apply cubes to an existing solver with
an internal CNF formula is to just copy the solver and apply each new
cube with the same path to this copied instance. A deeper cube must be applied by stepping through the
CNF tree and applying all cubes on the path from the root formula until the desired target cube.
