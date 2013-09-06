emcc --bind -o src/ogdf.js/ogdf.js src/ogdf.js/bindings/bindings.cpp src/ogdf/basic/Graph.cpp src/ogdf/basic/geometry.cpp src/ogdf/basic/basic.cpp src/ogdf/basic/EdgeComparerSimple.cpp src/ogdf/basic/GraphCopy.cpp src/ogdf/basic/PoolMemoryAllocator.cpp src/ogdf/basic/graph_generators.cpp src/ogdf/basic/graphics.cpp src/ogdf/basic/GraphAttributes.cpp src/ogdf/fileformats/GraphIO.cpp src/ogdf/fileformats/GraphIO_svg.cpp src/ogdf/fileformats/GmlParser.cpp src/ogdf/layered/acyclic_subgraph.cpp src/ogdf/basic/simple_graph_alg.cpp src/ogdf/layered/sugiyama.cpp src/ogdf/layered/ranking.cpp src/ogdf/layered/FastHierarchyLayout.cpp src/ogdf/layered/SugiyamaLayoutMC.cpp src/ogdf/basic/Hashing.cpp src/ogdf/layered/OptimalHierarchyClusterLayout.cpp src/ogdf/tree/TreeLayout.cpp src/ogdf/energybased/FMMMLayout.cpp src/ogdf/energybased/FruchtermanReingold.cpp src/ogdf/energybased/NMM.cpp src/ogdf/energybased/NodeAttributes.cpp src/ogdf/energybased/EdgeAttributes.cpp src/ogdf/energybased/FMEThread.cpp src/ogdf/basic/Thread.cpp src/ogdf/basic/System.cpp src/ogdf/basic/Stopwatch.cpp src/ogdf/energybased/UniformGrid.cpp src/ogdf/energybased/SpringEmbedderKK.cpp src/ogdf/energybased/Multilevel.cpp src/ogdf/energybased/numexcept.cpp src/ogdf/energybased/FruchtermanReingold.cpp src/ogdf/energybased/MAARPacking.cpp -Iinclude -Isrc/ogdf/energybased
