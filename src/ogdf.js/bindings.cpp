#include <emscripten/bind.h>
using namespace ogdf;
using namespace emscripten;
EMSCRIPTEN_BINDINGS(graph) {
    class_<ogdf::Graph>("Graph")
		.constructor()		
		.function("numberOfNodes",&ogdf::Graph::numberOfNodes)
		.function("numberOfEdges",&ogdf::Graph::numberOfEdges)
		.function("empty",&ogdf::Graph::empty)
		.function("newNode", select_overload<ogdf::node()>(&ogdf::Graph::newNode),allow_raw_pointers())
		.function("newNode",select_overload<ogdf::node(int)>(&ogdf::Graph::newNode),allow_raw_pointers())
	;
	function("randomGraph", &ogdf::randomGraph);
}
