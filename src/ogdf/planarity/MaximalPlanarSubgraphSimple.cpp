/*
 * $Revision: 3188 $
 *
 * last checkin:
 *   $Author: gutwenger $
 *   $Date: 2013-01-10 14:23:32 +0530 (Thu, 10 Jan 2013) $
 ***************************************************************/

/** \file
 * \brief Implementation of class MaximalPlanarSubgraphSimple.
 *
 * \author Carsten Gutwenger
 *
 * \par License:
 * This file is part of the Open Graph Drawing Framework (OGDF).
 *
 * \par
 * Copyright (C)<br>
 * See README.txt in the root directory of the OGDF installation for details.
 *
 * \par
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * Version 2 or 3 as published by the Free Software Foundation;
 * see the file LICENSE.txt included in the packaging of this file
 * for details.
 *
 * \par
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * \par
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * \see  http://www.gnu.org/copyleft/gpl.html
 ***************************************************************/


#include <ogdf/planarity/MaximalPlanarSubgraphSimple.h>
#include <ogdf/basic/extended_graph_alg.h>


namespace ogdf {


// default constructor
MaximalPlanarSubgraphSimple::MaximalPlanarSubgraphSimple()
{
}


// copy constructor
MaximalPlanarSubgraphSimple::MaximalPlanarSubgraphSimple(const MaximalPlanarSubgraphSimple &mps)
	: PlanarSubgraphModule(mps)
{
}


// clone method
PlanarSubgraphModule *MaximalPlanarSubgraphSimple::clone() const
{
	return new MaximalPlanarSubgraphSimple(*this);
}


// assignment operator
MaximalPlanarSubgraphSimple &MaximalPlanarSubgraphSimple::operator=(const MaximalPlanarSubgraphSimple &mps)
{
	m_timeLimit = mps.m_timeLimit;
	return *this;
}


Module::ReturnType MaximalPlanarSubgraphSimple::doCall(
	const Graph &G,
	const List<edge> &preferedEdges,
	List<edge> &delEdges,
	const EdgeArray<int>  * /* pCost */,
	bool preferedImplyPlanar)
{
	delEdges.clear();

	Graph H;
	NodeArray<node> mapToH(G);

	node v;
	forall_nodes(v,G)
		mapToH[v] = H.newNode();

	EdgeArray<bool> visited(G,false);

	ListConstIterator<edge> it;
	for(it = preferedEdges.begin(); it.valid(); ++it)
	{
		edge eG = *it;
		visited[eG] = true;

		edge eH = H.newEdge(mapToH[eG->source()],mapToH[eG->target()]);

		if (preferedImplyPlanar == false && isPlanar(H) == false) {
			H.delEdge(eH);
			delEdges.pushBack(eG);
		}
	}

	edge eG;
	forall_edges(eG,G)
	{
		if(visited[eG] == true)
			continue;

		edge eH = H.newEdge(mapToH[eG->source()],mapToH[eG->target()]);

		if (isPlanar(H) == false) {
			H.delEdge(eH);
			delEdges.pushBack(eG);
		}
	}

	return retFeasible;
}


} // end namespace ogdf
