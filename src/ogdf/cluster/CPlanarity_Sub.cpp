/*
 * $Revision: 3526 $
 *
 * last checkin:
 *   $Author: gutwenger $
 *   $Date: 2013-05-31 20:00:06 +0530 (Fri, 31 May 2013) $
 ***************************************************************/

/** \file
 * \brief Implementation of the subproblem class for the Branch&Cut algorithm
 * for the Maximum C-Planar SubGraph problem
 * Contains separation algorithms as well as primal heuristics.
 *
 * \authors Markus Chimani, Karsten Klein
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

#include <ogdf/basic/basic.h>

#ifdef USE_ABACUS

#include <ogdf/internal/cluster/CPlanarity_Sub.h>
#include <ogdf/internal/cluster/CPlanar_Edge.h>
#include <ogdf/internal/cluster/KuratowskiConstraint.h>
#include <ogdf/internal/cluster/Cluster_CutConstraint.h>
#include <ogdf/internal/cluster/Cluster_ChunkConnection.h>
#include <ogdf/internal/cluster/Cluster_MaxPlanarEdges.h>
#include <ogdf/graphalg/MinimumCut.h>
#include <ogdf/basic/BinaryHeap2.h>
#include <ogdf/cluster/CconnectClusterPlanar.h>
#include <ogdf/basic/extended_graph_alg.h>
#include <ogdf/basic/MinHeap.h>
#include <ogdf/basic/ArrayBuffer.h>
#include <ogdf/fileformats/GraphIO.h>

#include <ogdf/abacus/lpsub.h>
#include <ogdf/abacus/setbranchrule.h>

//output intermediate results when new sons are generated
//#define IM_OUTPUT
#ifdef IM_OUTPUT
#include <ogdf/basic/GraphAttributes.h>
#endif

using namespace ogdf;
using namespace abacus;


CPlanaritySub::CPlanaritySub(Master *master) : Sub(master,500,((CPlanarityMaster*)master)->m_inactiveVariables.size(),2000,false), detectedInfeasibility(false), inOrigSolveLp(false), bufferedForCreation(10) {
	m_constraintsFound = false;
	m_sepFirst = false;
}


CPlanaritySub::CPlanaritySub(Master *master,Sub *father,BranchRule *rule,List<Constraint*>& criticalConstraints) :
Sub(master,father,rule), detectedInfeasibility(false), inOrigSolveLp(false), bufferedForCreation(10) {
	m_constraintsFound = false;
	m_sepFirst = false;
	criticalSinceBranching.exchange(criticalConstraints); // fast load
	Logger::slout() << "Construct Child Sub " << id() << "\n";
}


CPlanaritySub::~CPlanaritySub() {}


Sub *CPlanaritySub::generateSon(BranchRule *rule) {
	//dualBound_ = realDualBound;

	const double minViolation = 0.001; // value fixed from abacus...
	List< Constraint* > criticalConstraints;
	if (master()->pricing())
	{
		//SetBranchRule* srule = (SetBranchRule*)(rule);
		SetBranchRule* srule;
		srule = dynamic_cast<SetBranchRule*>(rule);
		OGDF_ASSERT( srule ); // hopefully no other branching stuff...
		//Branching by setting a variable to 0 may
		//result in infeasibility of the current system
		//because connectivity constraints may not be feasible
		//with the current set of variables
		if(!srule->setToUpperBound()) { // 0-branching
			int varidx = srule->variable();
			EdgeVar* var = (EdgeVar*)variable(varidx);

			Logger::slout() << "FIXING VAR: ";
			var->printMe(Logger::slout());
			Logger::slout() << "\n";

			for(int i = nCon(); i-->0;) {
				Constraint* con = constraint(i);
				double coeff = con->coeff(var);
				if(con->sense()->sense()==CSense::Greater && coeff>0.99) {
					// check: yVal gives the slack and is always negative or 0
					double slk;
					//slk = yVal(i);
					slk = con->slack(actVar(),xVal_);
					//quick hack using ABACUS value, needs to be corrected
					if (slk > 0.0 && slk < minViolation)
					{
						slk = 0.0;
					}
					if(slk > 0.0) {
						Logger::slout() << "ohoh..." << slk << " "; var->printMe(Logger::slout()); Logger::slout()<<flush;
					}
					OGDF_ASSERT( slk <= 0.0 )
					double zeroSlack = slk+xVal(varidx)*coeff;
					if(zeroSlack > 0.0001) { // setting might introduce infeasibility
	// TODO: is the code below valid (in terms of theory) ???
	// "es reicht wenn noch irgendeine nicht-auf-0-fixierte variable im constraint existiert, die das rettet
	// mögliches problem: was wenn alle kanten bis auf die aktive kante in einem kuratowski constraint
	// auf 1 fixiert sind (zB grosse teile wegen planaritätstest-modus, und ein paar andere wg. branching).
	//
	//					bool good = false; // does there exist another good variable?
	//					for(int j = nVar(); j-->0;) {
	//						if(con->coeff(variable[j])>0.99 && VARIABLE[j]-NOT-FIXED-TO-0) {
	//							good = true;
	//							break;
	//						}
	//					}
	//					if(!good)
						criticalConstraints.pushBack(con);
					}
				}
			}
		}
	}//pricing

	return new CPlanaritySub(master_, this, rule, criticalConstraints);
}


int CPlanaritySub::selectBranchingVariable(int &variable) {
	//dualBound_ = realDualBound;

	return Sub::selectBranchingVariable(variable);
}


int CPlanaritySub::selectBranchingVariableCandidates(ArrayBuffer<int> &candidates) {
//	if(master()->m_checkCPlanar)
//		return Sub::selectBranchingVariableCandidates(candidates);

	ArrayBuffer<int> candidatesABA(1,false);
	int found = Sub::selectBranchingVariableCandidates(candidatesABA);

	if (found == 1) return 1;
	else {
		int i = candidatesABA.popRet();
		candidates.push(i);

		return 0;
	}
}


void CPlanaritySub::updateSolution() {

	List<nodePair> connectionOneEdges;
	nodePair np;

	for (int i=0; i<nVar(); ++i) {
		if (xVal(i) >= 1.0-(master_->eps())) {

			EdgeVar *e = (EdgeVar*)variable(i);
			np.v1 = e->sourceNode();
			np.v2 = e->targetNode();
			connectionOneEdges.pushBack(np);
			//cout << "******************************************************\n";
		}
	}
#ifdef OGDF_DEBUG
	((CPlanarityMaster*)master_)->m_solByHeuristic = false;
#endif
	((CPlanarityMaster*)master_)->updateBestSubGraph(connectionOneEdges);
}

//KK Uh this is extremely slow
CPlanaritySub::KuraSize CPlanaritySub::subdivisionLefthandSide(SListConstIterator<KuratowskiWrapper> kw, GraphCopy *gc, SListPure<nodePair> &subDivOrig) {
	subDivOrig.clear();
	KuraSize ks;
	ks.varnum = 0;
	ks.lhs = 0.0;
	node v,w;
//	for (int i=0; i<((CPlanarityMaster*)master_)->nVars(); ++i) {
	for (int i=0; i<nVar(); ++i) {
		EdgeVar *e = (EdgeVar*)variable(i);
		v = e->sourceNode();
		w = e->targetNode();
		SListConstIterator<edge> it;
		for (it=(*kw).edgeList.begin(); it.valid(); ++it) {
			if ( ((*it)->source() == gc->copy(v) && (*it)->target() == gc->copy(w) ) ||
				((*it)->source() == gc->copy(w) && (*it)->target() == gc->copy(v) ) ) {
					ks.lhs += xVal(i);
					ks.varnum++;
					nodePair pp;
					pp.v1 = v;
					pp.v2 = w;
					subDivOrig.pushBack(pp);
				}
		}
	}
	return ks;
}


///////////////////////////////////////////////////
//					HEURISTIC
///////////////////////////////////////////////////

// The code here should build a connected graph based on lp values,
// but for pure c-planarity testing we would need to add the original
// graph first, then check for additional connectivity that does
// not destroy planarity (solving our original problem)...
// As an alternative, one could try to solve the problem
// on a small subset of the connection edges, and also make
// use of the negative results in that case.
double CPlanaritySub::heuristicImprovePrimalBound(
	List<nodePair> &conEdges)
{
	//as long as there is no heuristic, we simulate failure
	return master_->primalBound();
}

int CPlanaritySub::improve(double &primalValue) {
	if (master()->feasibleFound())
	{
		cout << "Setting bounds due to feasibility\n";
		master()->dualBound(master()->primalBound());
		master()->primalBound(0);
	}
		//primalValue = dualBound();
	if ( ((CPlanarityMaster*)master_)->getHeuristicLevel() == 0 ||
			master()->feasibleFound() ) return 0;

	// If \a heuristicLevel is set to value 1, the heuristic is only run,
	// if current solution is fractional and no further constraints have been found.
	if ( ((CPlanarityMaster*)master_)->getHeuristicLevel() == 1 ) {
		if (!integerFeasible() && !m_constraintsFound) {

			List<nodePair> conEdges;


			for (int i=((CPlanarityMaster*)master_)->getHeuristicRuns(); i>0; i--) {

				conEdges.clear();
				double heuristic = heuristicImprovePrimalBound(conEdges);

				// \a heuristic contains now the objective function value (primal value)
				// of the heuristically computed ILP-solution.
				// We have to check if this solution is better than the currently best primal solution.
				if(master_->betterPrimal(heuristic)) {
#ifdef OGDF_DEBUG
	((CPlanarityMaster*)master_)->m_solByHeuristic = true;
#endif
					// Best primal solution has to be updated.
					((CPlanarityMaster*)master_)->updateBestSubGraph(conEdges);
					primalValue = heuristic;
					return 1;
				}
			}
			return 0;
		}

	// If \a heuristicLevel is set to value 2, the heuristic is run after each
	// LP-optimization step, i.e. after each iteration.
	} else if ( ((CPlanarityMaster*)master_)->getHeuristicLevel() == 2 ) {
		List<nodePair> conEdges;

		double heuristic = heuristicImprovePrimalBound(conEdges);

		// \a heuristic contains now the objective function value (primal value)
		// of the heuristically computed ILP-solution.
		// We have to check if this solution is better than the currently best primal solution.
		if(master_->betterPrimal(heuristic)) {
#ifdef OGDF_DEBUG
	((CPlanarityMaster*)master_)->m_solByHeuristic = true;
#endif
			// Best primal solution has to be updated
			((CPlanarityMaster*)master_)->updateBestSubGraph(conEdges);
			primalValue = heuristic;
			return 1;
		}
		return 0;
	}

	// For any other value of \a m_heuristicLevel the function returns 0.
	return 0;
}

//! Computes the number of bags within the given cluster \a c
//! (non recursive)
//! A bag is a set of chunks within the cluster that are connected
//! via subclusters
int CPlanaritySub::clusterBags(ClusterGraph &CG, cluster c)
{
	const Graph& G = CG.constGraph();
	if (G.numberOfNodes() == 0) return 0;
	int numChunks = 0; //number of chunks (CCs) within cluster c
	int numBags;       //number of bags (Constructs consisting of CCs connected by subclusters)

	//stores the nodes belonging to c
	List<node> nodesInCluster;
	((CPlanarityMaster*)master_)->getClusterNodes(c, nodesInCluster);
	//stores the corresponding iterator to the list element for each node
	NodeArray<ListIterator<node> > listPointer(G);

	NodeArray<bool> isVisited(G, false);
	NodeArray<bool> inCluster(G, false);
	NodeArray<node> parent(G); //parent for path to representative in bag gathering

	//saves status of all nodes in hierarchy subtree at c
	//c->getClusterNodes(nodesInCluster);
	int num = nodesInCluster.size();
	if (num == 0) return 0;

//    cout << "#Starting clusterBags with cluster of size " << num << "\n";

	//now we store the iterators
	ListIterator<node> it = nodesInCluster.begin();
	while (it.valid())
	{
		listPointer[(*it)] = it;
		inCluster[(*it)] = true;
		it++;
	}//while

	int count = 0;

	//now we make a traversal through the induced subgraph,
	//jumping between the chunks
	while (count < num)
	{
		numChunks++;
		node start = nodesInCluster.popFrontRet();

		//do a BFS and del all visited nodes in nodesInCluster using listPointer
		Queue<node> activeNodes; //could use arraybuffer
		activeNodes.append(start);
		isVisited[start] = true;
		edge e;
		while (!activeNodes.empty())
		{
			node v = activeNodes.pop(); //running node
			parent[v] = start; //representative points to itself
//            cout << "Setting parent of " << v->index() << "  to " << start->index() << "\n";
			count++;

			forall_adj_edges(e, v)
			{
				node w = e->opposite(v);

				if (v == w) continue; // ignore self-loops

				if (inCluster[w] && !isVisited[w])
				{
					//use for further traversal
					activeNodes.append(w);
					isVisited[w] = true;
					//remove the node from the candidate list
					nodesInCluster.del(listPointer[w]);
				}
			}
		}//while

	}//while

//    cout << "Number of chunks: " << numChunks << "\n";
	//Now all node parents point to the representative of their chunk (start node in search)
	numBags = numChunks; //We count backwards if chunks are connected by subcluster

	//Now we use an idea similar to UNION FIND to gather the bags
	//out of the chunks. Each node has a parent pointer, leading to
	//a representative. Initially, it points to the rep of the chunk,
	//but each time we encounter a subcluster connecting multiple
	//chunks, we let all of them point to the same representative.
	ListConstIterator<cluster> itC = c->cBegin();
	while (itC.valid())
	{
		const List<node> &nodesInChild = ((CPlanarityMaster*)master_)->getClusterNodes(*itC);
		//(*itC)->getClusterNodes(nodesInChild);
		//cout << nodesInChild.size() << "\n";
		ListConstIterator<node> itN = nodesInChild.begin();
		node bagRep; //stores the representative for the whole bag
		if (itN.valid()) bagRep = getRepresentative(*itN, parent);
//        cout << " bagRep is " << bagRep->index() << "\n";
		while (itN.valid())
		{
			node w = getRepresentative(*itN, parent);
//            cout << "  Rep is: " << w->index() << "\n";
			if (w != bagRep)
			{
				numBags--; //found nodes with different representative, merge
				parent[w] = bagRep;
				parent[*itN] = bagRep; //shorten search path
//                cout << "  Found new node with different rep, setting numBags to " << numBags << "\n";
			}
			itN++;
		}//While all nodes in subcluster

		itC++;
	}//while all child clusters

	return numBags;
//    cout << "#Number of bags: " << numBags << "\n";
}//clusterBags


//! returns connectivity status for complete connectivity
//! returns 1 in this case, 0 otherwise
// New version using arrays to check cluster affiliation during graph traversal,
// old version used graph copies

// For complete connectivity also the whole graph needs to
// be connected (root cluster). It therefore does not speed up
// the check to test connectivity of the graph in advance.
// Note that then a cluster induced graph always has to be
// connected to the complement, besides one of the two is empty.

// Uses an array that keeps the information on the cluster
// affiliation and bfs to traverse the graph.
//we rely on the fact that support is a graphcopy of the underlying graph
//with some edges added or removed
bool CPlanaritySub::checkCConnectivity(const GraphCopy& support)
{
	OGDF_ASSERT(isConnected(support));
	const ClusterGraph &CG = *((CPlanarityMaster*)master_)->getClusterGraph();
	const Graph& G = CG.constGraph();
	//if there are no nodes, there is nothing to check
	if (G.numberOfNodes() < 2) return true;

	cluster c;

	//there is always at least the root cluster
	forall_clusters(c, CG)
	{
		// For each cluster, the induced graph partitions the graph into two sets.
		// When the cluster is empty, we still check the complement and vice versa.
		bool set1Connected = false;

		//this initialization can be done faster by using the
		//knowledge of the cluster hierarchy and only
		//constructing the NA once for the graph (bottom up tree traversal)
		NodeArray<bool> inCluster(G, false);
		NodeArray<bool> isVisited(G, false);

		//saves status of all nodes in hierarchy subtree at c
		int num = c->getClusterNodes(inCluster);

		int count = 0;
		//search in graph should reach num and V-num nodes
		node complementStart = 0;

		//we start with a non-empty set
		node start = G.firstNode();
		bool startState = inCluster[start];

		Queue<node> activeNodes; //could use arraybuffer
		activeNodes.append(start);
		isVisited[start] = true;

		//could do a shortcut here for case |c| = 1, but
		//this would make the code more complicated without large benefit
		edge e;
		node u;
		while (!activeNodes.empty())
		{
			node v = activeNodes.pop(); //running node
			count++;
			u = support.copy(v);

			forall_adj_edges(e, u)
			{
				node w = support.original(e->opposite(u));

				if (v == w) continue; // ignore self-loops

				if (inCluster[w] != startState) complementStart = w;
				else if (!isVisited[w])
				{
					activeNodes.append(w);
					isVisited[w] = true;
				}
			}
		}//while
		//check if we reached all nodes
		//we assume that the graph is connected, otherwise check
		//fails for root cluster anyway
		//(we could have a connected cluster and a connected complement)

		//condition depends on the checked set, cluster or complement
		set1Connected = (startState == true ? (count == num) : (count == G.numberOfNodes() - num));

		//cout << "Set 1 connected: " << set1Connected << " Cluster? " << startState << " Cluster size: "<< num <<", have: "<< count <<"\n";
		//cout << "Root:" << (CG.rootCluster() == c) << "\n";

		if (!set1Connected) return false;
		//check if the complement of set1 is also connected
		//two cases: complement empty: ok
		//           complement not empty,
		//           but no complementStart found: error
		//In case of the root cluster, this always triggers,
		//therefore we have to continue
		if (G.numberOfNodes() == count)
			continue;
		OGDF_ASSERT(complementStart != 0);

		activeNodes.append(complementStart);
		isVisited[complementStart] = true;
		startState = ! startState;
		int ccount = 0;
		while (!activeNodes.empty())
		{
			node v = activeNodes.pop(); //running node
			ccount++;
			u = support.copy(v);

			forall_adj_edges(e, u)
			{
				node w = support.original(e->opposite(u));

				if (v == w) continue; // ignore self-loops

				if (!isVisited[w])
				{
					activeNodes.append(w);
					isVisited[w] = true;
				}
			}
		}//while
		//Check if we reached all nodes
		if (!(ccount + count == G.numberOfNodes()))
			return false;
	}//forallclusters
	return true;
}

//only left over for experimental evaluation of speedups
bool CPlanaritySub::checkCConnectivityOld(const GraphCopy& support)
{
	//Todo: It seems to me that this is not always necessary:
	//For two clusters, we could stop even if support is not connected
	if (isConnected(support)) {

		GraphCopy *cSupport;
		cluster c = ((CPlanarityMaster*)master_)->getClusterGraph()->firstCluster();

		while (c != NULL) {
			// Determining the nodes of current cluster
			List<node> clusterNodes;
			c->getClusterNodes(clusterNodes);

			// Step1: checking the restgraph for connectivity
			GraphCopy cSupportRest((const Graph&)support);
			ListIterator<node> it;
			node cv1, cv2;

			for (it=clusterNodes.begin(); it.valid(); ++it) {

				cv1 = support.copy(*it);
				cv2 = cSupportRest.copy(cv1);
				cSupportRest.delNode(cv2);
			}

			// Checking \a cSupportRest for connectivity
			if (!isConnected(cSupportRest)) {
				return false;
			}

			// Step2: checking the cluster induced subgraph for connectivity
			cSupport = new GraphCopy((const Graph&)support);
			NodeArray<bool> inCluster(*((CPlanarityMaster*)master_)->getGraph());
			inCluster.fill(false);

			for (it=clusterNodes.begin(); it.valid(); ++it) {
				inCluster[*it] = true;
			}
			node v = ((CPlanarityMaster*)master_)->getGraph()->firstNode();
			node succ;
			while (v!=0) {
				succ = v->succ();
				if (!inCluster[v]) {
					cv1 = support.copy(v);
					cv2 = cSupport->copy(cv1);
					cSupport->delNode(cv2);
				}
				v = succ;
			}
			if (!isConnected(*cSupport)) {
				return false;
			}
			delete cSupport;

			// Next cluster
			c = c->succ();
		}

	} else {
		return false;
	}
	return true;

}

bool CPlanaritySub::feasible() {
//	cout << "Checking feasibility\n";

	if (!integerFeasible()) {
		return false;
	}
	else {

		//----------------------------------------------------------------
		// Checking if the solution induced graph is completely connected.
		GraphCopy support(*((CPlanarityMaster*)master_)->getGraph());
		intSolutionInducedGraph(support);

		//introduced merely for debug checks
		bool cc = checkCConnectivity(support);
#ifdef OGDF_DEBUG
		bool ccOld = checkCConnectivityOld(support);
		if (cc != ccOld)
		{
			cout << "CC: "<<cc<<" CCOLD: "<<ccOld<<"\n";
			GraphIO::writeGML(support, "DifferingCC.gml");

		}
#endif
		OGDF_ASSERT (cc == ccOld);
		if (!cc) return false;

		//------------------------
		// Checking if the solution induced graph is planar.

		if (BoyerMyrvold().isPlanarDestructive(support)) {

			// Current solution is integer feasible, completely connected and planar.
			// We are done then, but for further handling of the result and any
			// extensions to the original code, we don't use a shortcut here.
			// Checking, if the objective function value of this subproblem is > than
			// the current optimal primal solution.
			// If so, the solution induced graph is updated.
			// We only got integer costs here.
#ifdef OGDF_DEBUG
			cout << "***Found valid Solution, check for improvement***\n";
#endif

			double primalBoundValue = lp_->value();
			if (master_->betterPrimal(primalBoundValue)) {
				master_->primalBound(primalBoundValue);
				updateSolution();
			}
			return true;

		} else {
			return false;
		}
	}
}//feasible

/*
static void dfsIsConnected(node v, NodeArray<bool> &visited, int &count)
{
	count++;
	visited[v] = true;

	edge e;
	forall_adj_edges(e,v) {
		node w = e->opposite(v);
		if (!visited[w]) dfsIsConnected(w,visited,count);
	}
}

bool CPlanaritySub::fastfeasible() {

	if (!integerFeasible()) {
		return false;
	}
	else {

		// Checking if the solution induced Graph is completely connected.
		GraphCopy support(*((CPlanarityMaster*)master_)->getGraph());
		intSolutionInducedGraph(support);

		//Todo: It seems to me that this is not always necessary:
		//For two clusters, we could stop even if support is not connected
		//we also do not need the root cluster
		if (isConnected(support)) {

			GraphCopy *cSupport;
			cluster c = ((CPlanarityMaster*)master_)->getClusterGraph()->firstCluster();

			while (c != NULL)
			{
				if (c == ((CPlanarityMaster*)master_)->getClusterGraph().rootCluster())
				{
					//attention: does the rest of the code rely on the fact
					//that the root is connected
					// Next cluster
					c = c->succ();
					continue;
				}//if root cluster
				// Determining the nodes of current cluster
				List<node> clusterNodes;
				c->getClusterNodes(clusterNodes);

				int count = 0;
				NodeArray<bool> blocked(support, false);
				// Step1: checking the restgraph for connectivity
				ListIterator<node> it;
				node cv1, cv2;

				for (it=clusterNodes.begin(); it.valid(); ++it)
				{
					blocked[*it] = true;

				}

				// Checking \a cSupportRest for connectivity

				if (clusterNodes.size() < support.numberOfNodes())
				{
					//search for a node outside c
					//should be done more efficiently, rewrite this
					node runv = support->firstNode();
					while (blocked[runv] == true) {runv = runv->succ();}

					dfsIsConnected(runv,blocked,count);
					if (count != support.numberOfNodes()-clusterNodes.size())
						return false;
				}


				// Step2: checking the cluster induced subgraph for connectivity
				//NodeArray<bool> inCluster(*((CPlanarityMaster*)master_)->getGraph());
				//inCluster.fill(false);
				blocked.init(support, true);

				for (it=clusterNodes.begin(); it.valid(); ++it) {
					blocked[*it] = false;
				}
				node v = ((CPlanarityMaster*)master_)->getGraph()->firstNode();
				node succ;
				while (v!=0) {
					succ = v->succ();
					if (!inCluster[v]) {
						cv1 = support.copy(v);
						cv2 = cSupport->copy(cv1);
						cSupport->delNode(cv2);
					}
					v = succ;
				}
				if (!isConnected(*cSupport)) {
					return false;
				}
				delete cSupport;

				// Next cluster
				c = c->succ();
			}

		} else {
			return false;
		}

		// Checking for planarity

		BoyerMyrvold bm;
		bool planar = bm.planarDestructive(support);
		if (planar) {

			// Current solution is integer feasible, completely connected and planar.
			// Checking, if the objective function value of this subproblem is > than
			// the current optimal primal solution.
			// If so, the solution induced graph is updated.
			double primalBoundValue = (double)(floor(lp_->value()) + 0.79);
			if (master_->betterPrimal(primalBoundValue)) {
				master_->primalBound(primalBoundValue);
				updateSolution();
			}
			return true;

		} else {
			return false;
		}
	}
}//fastfeasible

*/
//Adds all connection edges represented by value 1 variables to the input (original) graph.
void CPlanaritySub::intSolutionInducedGraph(GraphCopy &support) {

	//edge e, ce;
	node v, w, cv, cw;
	for (int i=0; i<nVar(); ++i) {
		if ( xVal(i) >= 1.0-(master_->eps()) ) {

			// each variable represents a new connection for pure c-planarity testing
			// If Connection-variables have value == 1.0 they have to be ADDED to the support graph.
			v = ((EdgeVar*)variable(i))->sourceNode();
			w = ((EdgeVar*)variable(i))->targetNode();
			cv = support.copy(v);
			cw = support.copy(w);
			support.newEdge(cv,cw);
		}
	}
}


void CPlanaritySub::kuratowskiSupportGraph(GraphCopy &support, double low, double high) {

	//edge e, ce;
	node v, w, cv, cw;
	for (int i=0; i<nVar(); ++i) {

		if (xVal(i) >= high) {

			// If variables have value >= \a high they are ADDED to the support graph.
			v = ((EdgeVar*)variable(i))->sourceNode();
			w = ((EdgeVar*)variable(i))->targetNode();
			cv = support.copy(v);
			cw = support.copy(w);
			OGDF_ASSERT(!support.searchEdge(cv,cw));
			if (!support.searchEdge(cv,cw)) support.newEdge(cv,cw);
		} else if (xVal(i) > low) {
			// Value of current variable lies between \a low and \a high.

			// Variable is added/deleted randomized according to its current value.

			// Variable of type CONNECT is added with probability of xVal(i).

			double ranVal = randomDouble(0.0,1.0);
			if (ranVal < xVal(i)) {
				v = ((EdgeVar*)variable(i))->sourceNode();
				w = ((EdgeVar*)variable(i))->targetNode();
				cv = support.copy(v);
				cw = support.copy(w);
				if (!support.searchEdge(cv,cw)) support.newEdge(cv,cw);
			}
		}

	} // end for(int i=0; i<nVar(); ++i)
}


void CPlanaritySub::connectivitySupportGraph(GraphCopy &support, EdgeArray<double> &weight) {

	// Step 1+2: Create the support graph & Determine edge weights and fill the EdgeArray \a weight.
	// MCh: warning: modified by unifying both steps. performance was otherwise weak.
	//edge e, ce;
	node v, w, cv, cw;
	// Initialize weight array to original graph (all original edges are part of an extension,
	// therefore have value 1.0)
	weight.init(support, 1.0); //forall support.chain(var->theEdge()).front()
	// Add new edges for relevant variables
	for (int i=0; i<nVar(); ++i) {
		EdgeVar* var = ((EdgeVar*)variable(i));
		double val = xVal(i);
		//weight array entry is set for all nonzero values
		if (val > master()->eps()) {
			// Connection edges have to be added.
			v = var->sourceNode();
			w = var->targetNode();
			cv = support.copy(v);
			cw = support.copy(w);
			// These edges never exist at this point
			// (but this is just guaranteed by the calling code!)
			weight[ support.newEdge(cv,cw) ] = val;
		}
	}
}

//----------------------------Computation of Cutting Planes-------------------------//
//															       			        //
//Implementation and usage of separation algorithmns						        //
//for the Kuratowski- and the Connectivity- constraints						        //
//													  						        //
//----------------------------------------------------------------------------------//

int CPlanaritySub::separateReal(double minViolate) {
	((CPlanarityMaster*)master())->m_nSep++;
	// The number of generated and added constraints.
	// Each time a constraint is created and added to the buffer, the variable \a count is incremented.
	// When adding the created constraints \a nGenerated and \a count are checked for equality.
	int nGenerated = 0;
	int count = 0;
	m_constraintsFound = false;

	if(master()->useDefaultCutPool())
		nGenerated = constraintPoolSeparation(0,0,minViolate);
	if(nGenerated>0) return nGenerated;

	//-------------------------------CUT-SEPARATION--------------------------------------//

	// We first try to regenerate cuts from our cutpools
	nGenerated = separateConnPool(minViolate);
	if (nGenerated > 0)
	{
#ifdef OGDF_DEBUG
	Logger::slout()<<"con-regeneration.";
#endif
		return nGenerated;
	}

	// We create new cut constraints
	// support is the complete graph that stays constant throughout the separation step,
	// except for non-edges with value 0.
	GraphCopy support (*((CPlanarityMaster*)master())->getGraph());
	EdgeArray<double> w;

	//Add edges for variables with value > 0,
	//graph with edge weights w given by the LP values
	connectivitySupportGraph(support,w);
	#ifdef OGDF_DEBUG
	Logger::slout() << "Support graph size is : "<<support.numberOfEdges()<<" edges \n";
	Logger::slout() << "If this is close to #G+#variables then it would be better to directly compute mincut on search space graph instead of >0 value graph\n";
	#endif

	// Buffer for the constraints
	//int nClusters = (((CPlanarityMaster*)master_)->getClusterGraph())->numberOfClusters();
	//ArrayBuffer<Constraint *> cConstraints(master_,2*nClusters);

	GraphCopy *c_support; //complement of a cluster induced graph
	EdgeArray<double> c_w;
	cluster c;

	// Now use the masters full GraphCopy which was only
	// created for that special purpose, to get the cuts in the search space.
	const GraphCopy& ssg  = *((CPlanarityMaster*)master())->searchSpaceGraph();

	// INTER-CLUSTER CONNECTIVITY

	forall_clusters(c,*((CPlanarityMaster*)master_)->getClusterGraph()) {

		c_support = new GraphCopy((const Graph&)support);
		c_w.init(*c_support);

		// Copying edge weights to \a c_w.
		//KK Why is that done this way instead of
		//a direct assignment to the copies edge list front?
		List<double> weights;
		edge e,c_e;
		forall_edges(e,support) {
			weights.pushBack(w[e]);
		}
		ListConstIterator<double> wIt = weights.begin();
		forall_edges(c_e,*c_support) {
			if (wIt.valid()) c_w[c_e] = (*wIt);
			wIt++;
		}

		// Residue graph is determined and stored in \a c_support.
		const List<node> &clusterNodes = ((CPlanarityMaster*)master_)->getClusterNodes(c);
		//c->getClusterNodes(clusterNodes);
		ListConstIterator<node> it;
		node cCopy1, cCopy2;
		//slow and ugly, mainly to get code running
		NodeArray<bool> isDeleted(support, false);
		for (it=clusterNodes.begin(); it.valid(); ++it) {
			cCopy1 = support.copy(*it);
			cCopy2 = c_support->copy(cCopy1);
			c_support->delNode(cCopy2);

			isDeleted[cCopy1] = true;
		}


		//const GraphCopy* const ssgp = ((CPlanarityMaster*)master())->searchSpaceGraph();

		// Checking if Graph is connected.
		if (isConnected(*c_support)) {
			//Logger::slout() << "\n*** Create new cuts: Complement is connected**\n";
			MinCut mc(*c_support,c_w);
			List<edge> cutEdges;
			double mincutV = mc.minimumCut();
			//may find a cut with only additional connection edges
			if (mincutV < 1.0-master()->eps()-minViolate) {
#ifdef OGDF_DEBUG
				Logger::slout() << "\n*** Create new cuts: Complement is connected, small cut found**\n";
#endif
				// What we have right now is the cut defined by non-edges with value >0
				// For validity at all times we add all outgoing non-edges
				List<node> partNodes;
				mc.partition(partNodes);
				NodeArray<bool> inPart(*c_support, false);
				// Run through partition to mark vertices, then add edges
				ListConstIterator<node> itn = partNodes.begin();
				while (itn.valid()) {
					inPart[*itn] = true;
					itn++;
				}

				List<nodePair> cutNodePairs;
				nodePair np;

#ifdef OGDF_DEBUG
	Logger::slout() << "Search space graph in subproblem, original size: \n";
	cout << "Search space graph in subproblem, original size: \n";
	cout << "\t"<<ssg.numberOfNodes()<<" "<<ssg.numberOfEdges()<<((CPlanarityMaster*)master())->searchSpaceGraph()->numberOfEdges()<<"\n";
	cout << "\t"<<ssg.original().numberOfNodes()<<" "<<ssg.original().numberOfEdges()<<"\n";
	Logger::slout() << "\t"<<ssg.numberOfNodes()<<" "<<ssg.numberOfEdges()<<"\n";
	Logger::slout() << "\t"<<ssg.original().numberOfNodes()<<" "<<ssg.original().numberOfEdges()<<"\n";
#endif
				itn = partNodes.begin();
				while (itn.valid()) {
				//scan neighbourhood
					node sn = ssg.copy(support.original(c_support->original(*itn)));
					edge sne;
					forall_adj_edges(sne, sn) {
						node sno = sne->opposite(sn);
						if (!sno) continue;

						OGDF_ASSERT(ssg.original(sno));
						OGDF_ASSERT(support.copy(ssg.original(sno) ));
						node supv = support.copy(ssg.original(sno));
						if (isDeleted[supv]) continue;
#ifdef OGDF_DEBUG
						cout <<"sn graph (should be ssg) "<<sn->graphOf()<<"\n";
						cout << "ssg: "<<&ssg<<" "<<"\n";
						cout << "support: "<<&support<<"\n";
						cout << "csupport: "<<&(*c_support)<<"\n";
						cout << "sno "<<sno <<" "<<sno->graphOf()<<" "<<(sno->graphOf())<<"\n";
						cout  <<ssg.original(sno)<<" "<<ssg.original(sno)->graphOf()<<"\n";
#endif
						//node might be from cluster, ie deleted in c_support, this doesnt work
						if (! c_support->copy( supv) ) continue;

						#ifdef OGDF_DEBUG
						cout << ssg.original(sno)<<"\n";
												cout << (support.copy(ssg.original(sno) ))<<"\n";
												cout << "Inpart query: " <<(c_support->copy(support.copy(ssg.original(sno) ) ))<<" "<< (c_support->copy(support.copy(ssg.original(sno) ) ))->graphOf()<<"\n";
						cout << "Inpart graph:, c_support "<<inPart.graphOf()<<" "<<c_support<<"\n";
cout <<"ssg original (sno) "<<(ssg.original(sno))->graphOf()<<"\n";
cout << "support copy" <<support.copy(ssg.original(sno))->graphOf()<<"\n";
cout << "csupport copy"<<(c_support->copy(support.copy(ssg.original(sno))))->graphOf()<<"\n";
/*node vw;
forall_nodes(vw, *c_support)
{
	cout << "c_s node and original + graphs: "<<vw<<" "<<vw->graphOf()<<" : "<<c_support->original(vw)<<" "<<(c_support->original(vw))->graphOf()<<"\n";
}
forall_nodes(vw, support)
{
	cout << "s node and original + graphs: "<<vw<<" "<<vw->graphOf()<<" : "<<support.original(vw)<<" "<<(support.original(vw))->graphOf()<<"\n";
}*/
#endif
						if (!inPart[  c_support->copy(supv )])
						{
							np.v1 = ssg.original(sn);
							np.v2 = ssg.original(sno);
							cutNodePairs.pushBack(np);
						}
					}
					itn++;
				}
				//old version using only edges with value > 0
				/*mc.cutEdges(cutEdges,*c_support);

				ListConstIterator<edge> cutEdgesIt;
				node v,w,cv,cw,ccv,ccw;
				for (cutEdgesIt=cutEdges.begin();cutEdgesIt.valid();cutEdgesIt++) {
					v = (*cutEdgesIt)->source();
					w = (*cutEdgesIt)->target();
					cv = c_support->original(v);
					cw = c_support->original(w);
					ccv = support.original(cv);
					ccw = support.original(cw);
					np.v1 = ccv;
					np.v2 = ccw;
					cutNodePairs.pushBack(np);
				}
*/
				// Create constraint
				bufferedForCreation.push(new CutConstraint((CPlanarityMaster*)master(),this, cutNodePairs));
				count++;
			}

		}//end Graph is connected
		else {
			// Variables may be set to zero, leading to missing edges
			NodeArray<int> comp(*c_support);
			connectedComponents(*c_support,comp);
			List<node> partition;
			NodeArray<bool> isInPartition(*c_support);
			isInPartition.fill(false);
			node v;
			//KK: Can we have/use multiple cuts here
			//each time? Would all at once be more efficient?
			forall_nodes(v,*c_support) {
				if (comp[v] == 0) {
					partition.pushBack(v);
					isInPartition[v] = true;
				}
			}

			// Computing nodePairs defining the cut. Instead of creating just
			// any possible connection that crosses the cut, we only add edges from
			// the search space graph.
			// Actually this now makes case B the same as above, so the code can
			// be unified as soon as experiments confirm success...
			List<nodePair> cutEdges;
			ListConstIterator<node> it;
			nodePair np;
			for (it=partition.begin(); it.valid(); ++it) {

				//scan neighbourhood in search space graph
				node sn = ssg.copy(support.original(c_support->original(*it)));
				edge sne;
				forall_adj_edges(sne, sn) {
						node sno = sne->opposite(sn);
						if ((!sno) || (sno == sn)) continue;

						OGDF_ASSERT(ssg.original(sno));
						OGDF_ASSERT(support.copy(ssg.original(sno) ));
						node supv = support.copy(ssg.original(sno));
						if (isDeleted[supv]) continue; //there is no copy in c_support
						node cw = c_support->copy(supv);
						if (!isInPartition[cw])
						{
							np.v1 = ssg.original(sn);
							np.v2 = ssg.original(sno);
							cutEdges.pushBack(np);
						}
				}
			}
			/* Old version, using the complete graph
			List<nodePair> cutEdges;
			ListConstIterator<node> it;
			nodePair np;
			for (it=partition.begin(); it.valid(); ++it) {
				node w,cv,cw;
				forall_nodes(w,*c_support) {
					//KK How can *it be in the partition and (w == *it && !isinpartition[w])
					if ( (w!=(*it)) && !isInPartition[w] ) {
						cw = c_support->original(w);
						cv = c_support->original(*it);
						np.v1 = support.original(cw);
						np.v2 = support.original(cv);
						cutEdges.pushBack(np);
					}
				}
			}*/

			// Create cut-constraint
			bufferedForCreation.push(new CutConstraint((CPlanarityMaster*)master(), this, cutEdges)); // always violated enough
			count++;

		}//end Graph is not connected
		delete c_support;

	}//end forall_clusters


	//KK The following part has to be adopted for scanning in Search Space Graph as the part above.
	//This is basically the same computation and should therefore just be handled by the same piece
	//code instead of a copy. TODO
	// INTRA-CLUSTER CONNECTIVITY

	// The initial constraints can not guarantee the connectivity of a cluster.
	// Thus, for each cluster we have to check, if the induced Graph is connected.
	// If so, we compute the mincut and create a corresponding constraint.
	// Otherwise a constraint is created in the same way as above.

	forall_clusters(c,*((CPlanarityMaster*)master_)->getClusterGraph()) {

		// Cluster induced Graph is determined and stored in \a c_support.
		// KK Why not using the inducedgraph method for that?
		// Todo There is also faster code in ClusterAnalysis for that?
		ListConstIterator<node> it;
		const List<node> &clusterNodes = ((CPlanarityMaster*)master_)->getClusterNodes(c);
		//c->getClusterNodes(clusterNodes);
		//may use a version that also gives us the nodes
		//of the complement directly (mark cluster, forall_clusters
		//collect nodes of unmarked clusters) for slight speedup
		//or even deletes them in the same run

		c_support = new GraphCopy((const Graph&)support);
		c_w.init(*c_support);

		List<double> weights;
		edge e,c_e;
		forall_edges(e,support) {
			weights.pushBack(w[e]);
		}
		ListConstIterator<double> wIt = weights.begin();
		forall_edges(c_e,*c_support) {
			if (wIt.valid()) c_w[c_e] = (*wIt);
			wIt++;
		}

		NodeArray<bool> isInCluster(*c_support, false);

		node cv;
		for (it=clusterNodes.begin(); it.valid(); ++it) {
			cv = support.copy(*it);
			isInCluster[c_support->copy(cv)] = true;
		}

		// Delete complement and store deletion status in support
		NodeArray<bool> isDeleted(support, false);
		node v = c_support->firstNode();
		node succ;
		while (v!=0) {
			succ = v->succ();
			if (!isInCluster[v]) {
				isDeleted[c_support->original(v)] = true;
				c_support->delNode(v);
			}
			v = succ;
		}

		// Checking if Graph is connected.
		if (isConnected(*c_support)) {
			//Logger::slout() << "\n*** Create new cuts: Cluster is connected**\n";
			MinCut mc(*c_support,c_w);
			List<edge> cutEdges;
			double x = mc.minimumCut();
			if (x < 1.0-master()->eps()-minViolate) {
				//Logger:slout() << "\n*** Create new cuts: Cluster is connected, small cut found**\n";
				// We cannot use the cut directly, as it only gives the edges set to > 0 here.
				// We therefore compute the corresponding cut in the search space graph.
				List<node> partNodes;
				mc.partition(partNodes);

				NodeArray<bool> inPart(*c_support, false);
				// Run through partition to mark vertices, then add edges
				ListConstIterator<node> itn = partNodes.begin();
				while (itn.valid()) {
					inPart[*itn] = true;
					itn++;
				}

				List<nodePair> cutNodePairs;
				nodePair np;

				itn = partNodes.begin();
				while (itn.valid()) {
				//scan neighbourhood
					node sn = ssg.copy(support.original(c_support->original(*itn)));
					edge sne;
					forall_adj_edges(sne, sn) {
						node sno = sne->opposite(sn);
						if (!sno) continue;

						OGDF_ASSERT(ssg.original(sno));
						OGDF_ASSERT(support.copy(ssg.original(sno) ));
						node supv = support.copy(ssg.original(sno));
						if (isDeleted[supv]) continue;
						//node might be from cluster complement, ie deleted in c_support, this doesnt work
						if (! c_support->copy( supv) ) continue;

						if (!inPart[  c_support->copy(supv ) ])
						{
							np.v1 = ssg.original(sn);
							np.v2 = ssg.original(sno);
							cutNodePairs.pushBack(np);
						}
					}
					itn++;
				}
				/* old part
				mc.cutEdges(cutEdges,*c_support);
				List<nodePair> cutNodePairs;
				ListConstIterator<edge> cutEdgesIt;
				node v,w,cv,cw,ccv,ccw;
				nodePair np;
				for (cutEdgesIt=cutEdges.begin();cutEdgesIt.valid();cutEdgesIt++) {
					v = (*cutEdgesIt)->source();
					w = (*cutEdgesIt)->target();
					cv = c_support->original(v);
					cw = c_support->original(w);
					ccv = support.original(cv);
					ccw = support.original(cw);
					np.v1 = ccv;
					np.v2 = ccw;
					cutNodePairs.pushBack(np);
				}
				*/

				// Create constraint
				bufferedForCreation.push(new CutConstraint((CPlanarityMaster*)master(),this, cutNodePairs));
				count++;
			}
		}//end Graph is connected
		else {
			// Variables may be set to zero, leading to missing edges
			NodeArray<int> comp(*c_support);
			connectedComponents(*c_support,comp);
			List<node> partition;
			NodeArray<bool> isInPartition(*c_support);
			isInPartition.fill(false);
			node v;
			forall_nodes(v,*c_support) {
				if (comp[v] == 0) {
					partition.pushBack(v);
					isInPartition[v] = true;
				}
			}

			List<nodePair> cutEdges;
			ListConstIterator<node> it;
			nodePair np;
			for (it=partition.begin(); it.valid(); ++it) {

				//scan neighbourhood in search space graph
				node sn = ssg.copy(support.original(c_support->original(*it)));
				edge sne;
				forall_adj_edges(sne, sn) {
						node sno = sne->opposite(sn);
						if ((!sno) || (sno == sn)) continue;

						OGDF_ASSERT(ssg.original(sno));
						OGDF_ASSERT(support.copy(ssg.original(sno) ));
						node supv = support.copy(ssg.original(sno));
						if (isDeleted[supv]) continue; //there is no copy in c_support
						node cw = c_support->copy(supv);
						if (!isInPartition[cw])
						{
							np.v1 = ssg.original(sn);
							np.v2 = ssg.original(sno);
							cutEdges.pushBack(np);
						}
				}
				/*old version
				node w,cv,cw;
				forall_nodes(w,*c_support) {
					if ( (w!=(*it)) && !isInPartition[w] ) {
						cw = c_support->original(w);
						cv = c_support->original(*it);
						np.v1 = support.original(cw);
						np.v2 = support.original(cv);
						cutEdges.pushBack(np);
					}
				}
				*/
			}

			// Create Cut-constraint
			bufferedForCreation.push(new CutConstraint((CPlanarityMaster*)master(), this, cutEdges)); // always violated enough.

			count++;
		}//end Graph is not connected
		delete c_support;
	}

	// Adding constraints
	if(count>0) {
		if(master()->pricing())
			nGenerated = createVariablesForBufferedConstraints();
		if(nGenerated==0) {
			ArrayBuffer<Constraint*> cons(count,false);
			while(!bufferedForCreation.empty()) {
				Logger::slout() <<"\n"; ((CutConstraint*&)bufferedForCreation.top())->printMe(Logger::slout());
				cons.push( bufferedForCreation.popRet() );
			}
			OGDF_ASSERT( bufferedForCreation.size()==0 );
			nGenerated = addCutCons(cons);
			OGDF_ASSERT( nGenerated == count );
			master()->updateAddedCCons(nGenerated);
#ifdef OGDF_DEBUG
			cout << "Added "<<count<<" cuts\n";
#endif
		}
		m_constraintsFound = true;
		return nGenerated;
	}


	//------------------------KURATOWSKI-SEPARATION----------------------------//

	// We first try to regenerate cuts from our cutpools
	nGenerated = separateKuraPool(minViolate);
	if (nGenerated > 0) {
		Logger::slout()<<"kura-regeneration.";
		return nGenerated; //TODO: Check if/how we can proceed here
	}
	// Since the Kuratowski support graph is computed from fractional values, an extracted
	// Kuratowski subdivision might not be violated by the current solution.
	// Thus, the separation algorithm is run several times, each time checking if the first
	// extracted subdivision is violated.
	// If no violated subdivisions have been extracted after \a nKuratowskiIterations iterations,
	// the algorithm behaves like "no constraints have been found".

	GraphCopy *kSupport;
	SList<KuratowskiWrapper> kuratowskis;
	BoyerMyrvold *bm2;
	bool violatedFound = false;

	// The Kuratowski support graph is created randomized  with probability xVal (1-xVal) to 0 (1).
	// Because of this, Kuratowski-constraints might not be found in the current support graph.
	// Thus, up to \a m_nKSupportGraphs are computed and checked for planarity.

	for (int i=0; i<((CPlanarityMaster*)master_)->getNKuratowskiSupportGraphs(); ++i) {

		// If a violated constraint has been found, no more iterations have to be performed.
		if (violatedFound) break;

		kSupport = new GraphCopy (*((CPlanarityMaster*)master())->getGraph());
		OGDF_ASSERT(isSimpleUndirected(*kSupport)); // Graph has to be simple
		kuratowskiSupportGraph(*kSupport,((CPlanarityMaster*)master_)->getKBoundLow(),((CPlanarityMaster*)master_)->getKBoundHigh());
		OGDF_ASSERT(isSimpleUndirected(*kSupport)); // Graph has to be simple

		if (isPlanar(*kSupport)) {
			delete kSupport;
			continue;
		}

		int iteration = 1;
		while(((CPlanarityMaster*)master_)->getKIterations() >= iteration) {
			OGDF_ASSERT(isSimpleUndirected(*kSupport)); // Graph has to be simple
			// Testing support graph for planarity.
			bm2 = new BoyerMyrvold();
			bm2->planarEmbedDestructive(*kSupport, kuratowskis, ((CPlanarityMaster*)master_)->getNSubdivisions(),false,false,true);
			delete bm2;

			// Checking if first subdivision is violated by current solution
			// Performance should be improved somehow!!!
			// KK Todo Why is this code divided into first kura and the remainder?
			SListConstIterator<KuratowskiWrapper> kw = kuratowskis.begin();
			SListConstIterator<KuratowskiWrapper> succ;

			SListPure<nodePair> subDivOrig; //stores nodepairs for contained connection edges

			KuraSize ks = subdivisionLefthandSide(kw, kSupport, subDivOrig);
			double leftHandSide = ks.lhs;
			OGDF_ASSERT(subDivOrig.size() == ks.varnum); //just a remainder of incremental code completion, may remove varnum again
			// Only violated constraints are created and added
			// if \a leftHandSide is greater than the number of edges in subdivision -1, the constraint is violated by current solution.
			if (leftHandSide > ks.varnum-(1-master()->eps()-minViolate)) {

				violatedFound = true;
			#ifdef OGDF_DEBUG
cout << "Violated Kura found \n";
cout << "K5?  "<<(*kw).isK5()<<"\n";
SListConstIterator<edge> itke = (*kw).edgeList.begin();
while (itke.valid())
{
	cout << "Edge between "<<(*itke)->source() << "-"<<(*itke)->target() << " in supportgraph\n";
	itke++;
}
NodeArray<int> potDeg(support,0); //number of potential additionial edges
for (int i=0; i<nVar(); ++i) {
			node v = ((EdgeVar*)variable(i))->sourceNode();
			node w = ((EdgeVar*)variable(i))->targetNode();
			node cv = support.copy(v);
			node cw = support.copy(w);
			potDeg[cv]++;
			potDeg[cw]++;
cout <<"Variable "<<i<<" v,w "<<v->index()<<" "<<w->index()<<" cv,cw "<<cv->index()<<" "<<cw->index()<<"\n";
		}
node v;
forall_nodes(v, support){
cout << "Additional potential degree of: "<<v->index()<< " is "<<potDeg[v]<<"\n";
}
#endif
				// Buffer for new Kuratowski constraints
				ArrayBuffer<Constraint *> kConstraints(kuratowskis.size(),false);

				// Adding first Kuratowski constraint to the buffer.
				kConstraints.push(new ClusterKuratowskiConstraint ((CPlanarityMaster*)master(), subDivOrig.size(), subDivOrig));
				count++;

				// Checking further extracted subdivisions for violation.
				kw++;
				while(kw.valid()) {
					KuraSize ks = subdivisionLefthandSide(kw, kSupport, subDivOrig);
					leftHandSide = ks.lhs;

					if (leftHandSide > ks.varnum-(1-master()->eps()-minViolate)) {

						// Adding Kuratowski constraint to the buffer.
						kConstraints.push(new ClusterKuratowskiConstraint ((CPlanarityMaster*)master(), subDivOrig.size(), subDivOrig) );
						count++;
					}
					kw++;
				}

				// Adding constraints to the pool.
				for(int i=0; i<kConstraints.size(); ++i) {
					Logger::slout() <<"\n"; ((ClusterKuratowskiConstraint*&)kConstraints[i])->printMe(Logger::slout());
				}
				nGenerated += addKuraCons(kConstraints);
				if (nGenerated != count)
				cerr << "Number of added constraints doesn't match number of created constraints" << endl;
				break;

			} else {
				kuratowskis.clear();
				iteration++;
			}
		}
		delete kSupport;
	}

	if (nGenerated > 0) {
		((CPlanarityMaster*)master_)->updateAddedKCons(nGenerated);
		m_constraintsFound = true;
	}
	return nGenerated;
}

int CPlanaritySub::createVariablesForBufferedConstraints() {
	List<Constraint*> crit;
	for(int i = bufferedForCreation.size(); i-->0;) {
//		((CutConstraint*)bufferedForCreation[i])->printMe(); Logger::slout() << ": ";
		for(int j=nVar(); j-->0;) {
//			((EdgeVar*)variable(j))->printMe();
//			Logger::slout() << "=" << bufferedForCreation[i]->coeff(variable(j)) << "/ ";
			if(bufferedForCreation[i]->coeff(variable(j))!=0.0) {
//				Logger::slout() << "!";
				goto nope;
			}
		}
		crit.pushBack(bufferedForCreation[i]);
		nope:;
	}
	if(crit.size()==0) return 0;
	ArrayBuffer<ListIterator<nodePair> > creationBuffer(crit.size());
	forall_nonconst_listiterators(nodePair, npit, master()->m_inactiveVariables) {
		bool select = false;
		ListIterator<Constraint*> ccit = crit.begin();
		while(ccit.valid()) {
			if(((BaseConstraint*)(*ccit))->coeff(*npit)) {
				ListIterator<Constraint*> delme = ccit;
				++ccit;
				crit.del(delme);
				select = true;
			} else
				++ccit;
		}
		if(select) creationBuffer.push(npit);
		if(crit.size()==0) break;
	}
	if( crit.size() ) { // something remained here...
		for(int i = bufferedForCreation.size(); i-->0;) {
			delete bufferedForCreation[i];
		}
		detectedInfeasibility = true;
		return 0; // a positive value denotes infeasibility
	}
	OGDF_ASSERT(crit.size()==0);
	ArrayBuffer<Variable*> vars(creationBuffer.size(),false);
	master()->m_varsCut += creationBuffer.size();
	int gen = creationBuffer.size();
	for(int j = gen; j-->0;) {
		vars.push( master()->createVariable( creationBuffer[j] ) );
	}
	myAddVars(vars);
	return -gen;
}


// !! This function is incorrect (due to uninitialized usage of variable rc)       !!
// !! and cannot work correctly (seems to be a placeholder for further development) !!
// Therefore it has been commented out
//int CPlanaritySub::pricingReal(double minViolate) {
//	if(!master()->pricing()) return 0; // no pricing
//	Top10Heap<Prioritized<ListIterator<nodePair> > > goodVar(master()->m_numAddVariables);
//	forall_nonconst_listiterators(nodePair, it, master()->m_inactiveVariables) {
//		double rc;
//		CPlanarEdgeVar v(master(), master()->nextConnectCoeff(), (*it).v1, (*it).v2);
//		if(v.violated(rc) && rc>=minViolate) {
//			Prioritized<ListIterator<nodePair> > entry(it,rc);
//			goodVar.pushBlind( entry );
//		}
//	}
//
//	int nv = goodVar.size();
//	if(nv > 0) {
//		ArrayBuffer<Variable*> vars(nv,false);
//		for(int i = nv; i-->0;) {
//			ListIterator<nodePair> it = goodVar[i].item();
//			vars.push( master()->createVariable(it) );
//		}
//		myAddVars(vars);
//	}
//	return nv;
//}

int CPlanaritySub::repair() {
	//warning. internal abacus stuff BEGIN
	bInvRow_ = new double[nCon()];
	lp_->getInfeas(infeasCon_, infeasVar_, bInvRow_);
	//warning. internal abacus stuff END

	// only output begin
	Logger::slout() << "lpInfeasCon=" << lp_->infeasCon()->size()
		<< " var="<< infeasVar_
		<< " con="<< infeasCon_<< "\n";
	for(int i=0; i<nCon(); ++i)
		Logger::slout() << bInvRow_[i] << " " << flush;
	Logger::slout() << "\n" << flush;
	for(int i=0; i<nCon(); ++i) {
		if(bInvRow_[i]!=0) {
			Logger::slout() << bInvRow_[i] << ": " << flush;
			ChunkConnection* chc = dynamic_cast<ChunkConnection*>(constraint(i));
			if(chc) chc->printMe(Logger::slout());
			CutConstraint* cuc = dynamic_cast<CutConstraint*>(constraint(i));
			if(cuc) cuc->printMe(Logger::slout());
			ClusterKuratowskiConstraint* kc = dynamic_cast<ClusterKuratowskiConstraint*>(constraint(i));
			if(kc) kc->printMe(Logger::slout());
			Logger::slout() << "\n" << flush;
		}
	}
	// only output end

	int added = 0;
	ArrayBuffer<Variable*> nv(1,false);
	for(int i=0; i<nCon(); ++i) {
		if(bInvRow_[i]<0) { // negativ: infeasible cut or chunk constraint, or oversatisfies kura
			BaseConstraint* b = dynamic_cast<BaseConstraint*>(constraint(i));
			if(!b) continue; // was: oversatisfied kura. nothing we can do here
			OGDF_ASSERT(b);
			forall_nonconst_listiterators(nodePair, it, master()->m_inactiveVariables) {
				if(b->coeff(*it)) {
					Logger::slout() << "\tFeasibility Pricing: ";
					nv.push( master()->createVariable(it) );
					Logger::slout() << "\n";
					myAddVars(nv);
					added = 1;
					goto done;
				}
			}
		}
	}
done:
	//warning. internal abacus stuff BEGIN
	delete[] bInvRow_;
	//warning. internal abacus stuff END
	master()->m_varsKura += added;
	return added;
}

int CPlanaritySub::solveLp() {
	m_reportCreation = 0;
	const double minViolation = 0.001; // value fixed by abacus...

	Logger::slout() << "SolveLp\tNode=" << this->id() << "\titeration=" << this->nIter_ << "\n";


	if(master()->pricing() && id()>1 && nIter_==1) { // ensure that global variables are really added...
		StandardPool<Variable, Constraint>* vp = master()->varPool();
		int addMe = vp->number() - nVar();
		OGDF_ASSERT(addMe >=0 );
		if(addMe) {
			Logger::slout() << "A problem ocurred\n";
			Logger::slout() << nVar() << " variables of " << vp->number() << " in model. Fetching " << addMe << ".\n" << flush;
			//master()->activeVars->loadIndices(this); // current indexing scheme
			m_reportCreation = 0;
			for(int i=0; i<vp->size(); ++i ) {
				PoolSlot<Variable, Constraint> * slot = vp->slot(i);
				Variable* v = slot->conVar();
				if(v && !v->active()) {
					addVarBuffer_->insert(slot,true);
					--m_reportCreation;
				}
			}
			OGDF_ASSERT(m_reportCreation == -addMe);
			return 0; // rerun;
		}
	}


	if( master()->feasibleFound()) {
		Logger::slout() << "Feasible Solution Found. That's good enough! C-PLANAR\n";
		master()->clearActiveRepairs();
		return 1;
	}

	if(bufferedForCreation.size()) {
		m_reportCreation = bufferedForCreation.size();
		ArrayBuffer<Constraint*> cons(bufferedForCreation.size(),false);
		while(!bufferedForCreation.empty()) {
			((CutConstraint*&)bufferedForCreation.top())->printMe(Logger::slout());Logger::slout() <<"\n";
			cons.push( bufferedForCreation.popRet() );
		}
		OGDF_ASSERT( bufferedForCreation.size()==0 );
		addCutCons(cons);
		master()->updateAddedCCons(m_reportCreation);
		master()->clearActiveRepairs();
		return 0;
	}

	inOrigSolveLp = true;
	++(master()->m_solvesLP);
	int ret = Sub::solveLp();
	inOrigSolveLp = false;
	// ret > 0 means the subproblem is infeasible
	// In case we do pricing, we might try to repair this
	if(ret) {
		if(master()->pricing()) {
			if(criticalSinceBranching.size()) {
				ListIterator<nodePair> best;
				Array<ListIterator<Constraint*> > bestKickout;
				int bestCCnt = 0;
				forall_nonconst_listiterators(nodePair, nit, master()->m_inactiveVariables) {
					ArrayBuffer<ListIterator<Constraint*> > kickout(criticalSinceBranching.size());
					forall_nonconst_listiterators(Constraint*, cit, criticalSinceBranching) {
						BaseConstraint* bc = dynamic_cast<BaseConstraint*>(*cit);
						OGDF_ASSERT(bc);
						if( bc->coeff(*nit) > 0.99) {
							kickout.push(cit);
						}
					}
					if(kickout.size() > bestCCnt) {
						bestCCnt = kickout.size();
						best = nit;
						kickout.compactMemcpy(bestKickout);
					}
				}
				if(bestCCnt>0) {
					ArrayBuffer<Variable*> vars(1,false);
					vars.push( master()->createVariable(best) );
					myAddVars(vars);
					int i;
					forall_arrayindices(i,bestKickout)
						criticalSinceBranching.del(bestKickout[i]);
					m_reportCreation = -1;
					++(master()->m_varsBranch);
					master()->clearActiveRepairs();
					return 0;
				}
				criticalSinceBranching.clear(); // nothing helped... resorting to full repair
//				master()->clearActiveRepairs();
//				return 0;
			} //else {
			m_reportCreation = -repair();
			if(m_reportCreation<0) {
				++(master()->m_activeRepairs);
				return 1;//0;
			}
			//}
		}
		master()->clearActiveRepairs();
		//dualBound_ = +master()->infinity();
		//cout <<"**Set dualbound to minus infinity\n";


#ifdef OGDF_DEBUG
	//Bit of dummy code for usual debug: Run over inactive and connect vars
	/*
	forall_listiterators(nodePair, it, master()->m_inactiveVariables) {
	}
	for(int t = 0; t<nVar(); ++t) {
		EdgeVar* v = (EdgeVar*)(variable(t));
		if(v->theEdgeType()==EdgeVar::CONNECT) {
			if(lBound(t)==uBound(t)) {
				Logger::slout() << "VAR FIXED: ";
				v->printMe(Logger::slout());
				Logger::slout() << " TO " << lBound(t) << "\n";
			}
		}
	}
}*/
#endif //OGDF_DEBUG

		// infeasibleSub();
		Logger::slout() << "\tInfeasible\n";
		return 1; // report any errors
	}
	master()->clearActiveRepairs();
	OGDF_ASSERT( !lp_->infeasible() );
	//is set here for pricing only
	//if(master()->m_checkCPlanar) // was: master()->pricing()
	//dualBound_=master()->infinity();//666
	Logger::slout() << "\t\tLP-relaxation: " <<  lp_->value() << "\n";
	Logger::slout() << "\t\tLocal/Global dual bound: " << dualBound() << "/" << master_->dualBound() << endl;
	realDualBound = lp_->value();

	//KK Is there a way to find a better corresponding shortcut here?
	//if(dualBound()>3*master()->m_G->numberOfEdges()-6) {
	//	dualBound(-master()->infinity());
	//	return 1;
	//}


	if(!master()->pricing()) {
		m_reportCreation = separateReal(minViolation);//use ...O for output

	} else {
		// Pricing-code has been disabled since it is currently incorrect!
		// See CPlanaritySub::pricingReal() above for more details.
		OGDF_THROW(AlgorithmFailureException);

		//m_sepFirst = !m_sepFirst;
		//if(m_sepFirst) {
		//	if( (m_reportCreation = separateRealO(master()->m_strongConstraintViolation)) ) return 0;
		//	if( detectedInfeasibility ) { Logger::slout() << "Infeasibility detected (a)"<< endl; return 1; }
		//	if( (m_reportCreation = -pricingRealO(master()->m_strongVariableViolation)) ) return 0;
		//	if( (m_reportCreation = separateRealO(minViolation)) ) return 0;
		//	if( detectedInfeasibility ) { Logger::slout() << "Infeasibility detected (b)"<< endl; return 1; }
		//	m_reportCreation = -pricingRealO(minViolation);
		//} else {
		//	if( (m_reportCreation = -pricingRealO(master()->m_strongVariableViolation)) ) return 0;
		//	if( (m_reportCreation = separateRealO(master()->m_strongConstraintViolation)) ) return 0;
		//	if( detectedInfeasibility ) { Logger::slout() << "Infeasibility detected (c)"<< endl; return 1; }
		//	if( (m_reportCreation = -pricingRealO(minViolation)) ) return 0;
		//	m_reportCreation = separateRealO(minViolation);
		//	if( detectedInfeasibility ) { Logger::slout() << "Infeasibility detected (d)"<< endl; return 1; }
		//}
	}
	return 0;
}


#endif // USE_ABACUS
