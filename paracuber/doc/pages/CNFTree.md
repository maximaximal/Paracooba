CNF Tree {#CNFTree}
===================

The CNFTree concept is the core concept behind coordinating the parallel cubing over multiple
compute nodes. The formula is divided into multiple paths to take
while cubing, each path is made unique using the sequence of choices combined
with the depth of the tree required to arrive at the desired location.

@dotfile cnftree-schematic.dot

Root Formula
------------

The root formula is the one with its `previous` field being equal to `0`. This
means `depth=0` and `path=0`. This is the main formula (in DIMACS format) given to
the application. Other cubes are attached to this formula afterwards.

Cubes
-----

Cubes branch off of the root formula and go down into the tree. A cube is a disjunction of
literals, which is represented internally by a `std::vector` of `int32_t` numbers inside of
the @ref paracuber::CNF class.

Applying Cubes to a solver inside a paracuber::CaDiCaLTask
----------------------------------------------------------

To apply a cube to a solver, the previous solver is copied and the new cube is applied. This
prevents the solver from re-parsing the entire tree and only changes need to be applied. Every
branch can also be parallelised. This step happens in paracuber::CaDiCaLTask::readCNF.
