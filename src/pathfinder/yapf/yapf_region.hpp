/** @file yapf_region.cpp */
#ifndef  YAPF_REGION_HPP
#define  YAPF_REGION_HPP

#include "../../stdafx.h"
#include "region.hpp"
#include "region_manager.hpp"
#include <vector>

using std::vector;

#include "yapf.hpp"

/** Yapf Node Key that evaluates hash from region pointer. */
template<class TRegionDescription>
struct CYapfNodeKeyRegion {

	typedef TRegionDescription TRD;

	CRegion<TRegionDescription> *m_region;

	inline void Set(CRegion<TRegionDescription> *reg)
	{
		m_region = reg;
	}

	inline int CalcHash() const {return m_region->GetIndex();}
	inline bool operator == (const CYapfNodeKeyRegion& other) const {return (m_region == other.m_region);}
};

/** Yapf Node for regions */
/** Yapf Node base */
template <class Tkey_>
struct CYapfRegionNodeT {
	typedef Tkey_ Key;
	typedef CYapfRegionNodeT<Tkey_> Node;

	Tkey_       m_key;
	Node       *m_hash_next;
	Node       *m_parent;
	int         m_cost;
	int         m_estimate;

	inline void Set(Node *parent, CRegion<typename Key::TRD>* reg)
	{
		m_key.Set(reg);
		m_hash_next = NULL;
		m_parent = parent;
		m_cost = 0;
		m_estimate = 0;
	}

	inline Node *GetHashNext() {return m_hash_next;}
	inline void SetHashNext(Node *pNext) {m_hash_next = pNext;}
	inline CRegion<typename Key::TRD> *GetRegion() const {return m_key.m_region;}
	inline const Tkey_& GetKey() const {return m_key;}
	inline int GetCost() {return m_cost;}
	inline int GetCostEstimate() {return m_estimate;}
	inline bool operator < (const Node& other) const {return m_estimate < other.m_estimate;}

	/*void Dump(DumpTarget &dmp) const
	{
		dmp.WriteStructT("m_key", &m_key);
		dmp.WriteStructT("m_parent", m_parent);
		dmp.WriteLine("m_cost = %d", m_cost);
		dmp.WriteLine("m_estimate = %d", m_estimate);
	}*/
};

/** YAPF origin for regions */
template <class Types>
class CYapfOriginRegionT
{
public:
	typedef typename Types::Tpf Tpf;              ///< the pathfinder class (derived from THIS class)
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;               ///< key to hash tables

protected:
	CRegion<typename Key::TRD>	*m_origin;                       ///< origin tile

	/// to access inherited path finder
	inline Tpf& Yapf() {return *static_cast<Tpf*>(this);}

public:
	/// Set origin region
	void SetOrigin(CRegion<typename Key::TRD>* origin)
	{
		m_origin = origin;
	}

	/// Called when YAPF needs to place origin nodes into open list
	void PfSetStartupNodes()
	{
			Node& n1 = Yapf().CreateNewNode();
			n1.Set(NULL,m_origin);
			Yapf().AddStartupNode(n1);
	}
};

/** YAPF destination provider for regions*/
template <class Types>
class CYapfDestinationRegionT
{
public:
	typedef typename Types::Tpf Tpf;              ///< the pathfinder class (derived from THIS class)
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;               ///< key to hash tables

protected:
	CRegion<typename Key::TRD>	*m_dest;                      ///< destination region

public:
	/// set the destination tile / more trackdirs
	void SetDestination(CRegion<typename Key::TRD>* dest)
	{
		m_dest = dest;
	}

protected:
	/// to access inherited path finder
	Tpf& Yapf() {return *static_cast<Tpf*>(this);}

public:
	/// Called by YAPF to detect if node ends in the desired destination
	inline bool PfDetectDestination(Node& n)
	{
		return n.GetRegion() == m_dest;
	}

	/** Called by YAPF to calculate cost estimate. Calculates distance to the destination
	 *  adds it to the actual cost from origin and stores the sum to the Node::m_estimate */
	inline bool PfCalcEstimate(Node& n)
	{
		if (PfDetectDestination(n)) {
			n.m_estimate = n.m_cost;
			return true;
		}
		n.m_estimate = n.m_cost + n.GetRegion()->DistanceTo(m_dest);
		if(!(n.m_estimate >= n.m_parent->m_estimate))
			++n.m_estimate;
		assert(n.m_estimate >= n.m_parent->m_estimate);
		return true;
	}
};

/** Node Follower module of YAPF for region pathfinding */
template <class Types>
class CYapfFollowRegionT
{
public:
	typedef typename Types::Tpf Tpf;                     ///< the pathfinder class (derived from THIS class)
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node;        ///< this will be our node type
	typedef typename Node::Key Key;                      ///< key to hash tables

protected:
	/// to access inherited path finder
	inline Tpf& Yapf() {return *static_cast<Tpf*>(this);}

public:
	/** Called by YAPF to move from the given node to the next tile. For each
	 *  reachable region on the new region creates new node, initializes it
	 *  and adds it to the open list by calling Yapf().AddNewNode(n) */
	inline void PfFollowNode(Node& old_node)
	{
		set<CRegion<typename Key::TRD>*> temp = old_node.GetRegion()->Neighbours()->Get();
		for (typename set<CRegion<typename Key::TRD>*>::const_iterator i = temp.begin();
		i != temp.end();
		++i){
			Node& n1 = Yapf().CreateNewNode();
			n1.Set(&old_node,*i);
			Yapf().AddNewNode(n1, TrackFollower());
		}
	}

	/// return debug report character to identify the transportation type
	inline char TransportTypeChar() const {return '^';}

	static vector<TileIndex> ChooseIntermediateDestinations(const typename Key::TRD::RegionVehicleType *v, TileIndex start, TileIndex end, uint regions_ahead, bool* path_not_found)
	  {
		assert(Key::TRD::IsRoutable(start) && Key::TRD::IsRoutable(end));
		if (Key::TRD::updates_active == false){
			Key::TRD::updates_active = true;
			CRegionManager<CRegion<typename Key::TRD> >::GetManager()->FindRegionsFromScratch();
		}

		assert(Key::TRD::GetRegion(start) != NULL && Key::TRD::GetRegion(end) != NULL);

		// handle special case - when start region is end region
		if (Key::TRD::GetRegion(start) == Key::TRD::GetRegion(end))
			return vector<TileIndex>(1,end);

		// create pathfinder instance
		Tpf pf;
		// set origin and destination nodes
		pf.SetOrigin(Key::TRD::GetRegion(start));
		pf.SetDestination(Key::TRD::GetRegion(end));
		// find best path - if no path is found return the start
		bool path_found = pf.FindPath(v);
		if (path_not_found != NULL && !path_found) {
			// tell controller that no route was found
			*path_not_found = true;
			return vector<TileIndex>(1,start);
		}

		vector<TileIndex> route;
		Node *pNode = pf.GetBestNode();
		if (pNode != NULL) {
			// walk through the path back to the origin recording the route
//			Node *pPrevNode = NULL;
			while (pNode->m_parent != NULL) {
				route.push_back(pNode->GetRegion()->GetCenter());
//				pPrevNode = pNode;
				pNode = pNode->m_parent;
			}
			//Return the amount of route requsted
			if (route.size() > regions_ahead)
				return vector<TileIndex>(route.end() - regions_ahead,route.end());
			else
				return vector<TileIndex>(route.begin(),route.end());
		}
		return vector<TileIndex>(1,end);
	}
};
/** Cost Provider module of YAPF for regions */
template <class Types>
class CYapfCostRegionT
{
public:
	typedef typename Types::Tpf Tpf;              ///< the pathfinder class (derived from THIS class)
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;               ///< key to hash tables

protected:
	/// to access inherited path finder
	Tpf& Yapf() {return *static_cast<Tpf*>(this);}

public:
	/** Called by YAPF to calculate the cost from the origin to the given node.
	 *  Calculates only the cost of given node, adds it to the parent node cost
	 *  and stores the result into Node::m_cost member */
	inline bool PfCalcCost(Node& n, const TrackFollower *tf)
	{
		//Add one on to the cost to prevent underestimation (only = 10% tile length)
		n.m_cost = n.m_parent->m_cost + n.GetRegion()->DistanceTo(n.m_parent->GetRegion()) + 1;
		return true;
	}
};

//Region does not need follower
struct NullFollower : public CFollowTrackWater
{
	int g;
};

/** Config struct of YAPF for route planning.
 *  Defines all 6 base YAPF modules as classes providing services for CYapfBaseT.
 */
template <class Tpf_, class Tnode_list>
struct CYapfRegion_TypesT
{
	/** Types - shortcut for this struct type */
	typedef CYapfRegion_TypesT<Tpf_, Tnode_list> Types;

	/** Tpf - pathfinder type */
	typedef Tpf_                              Tpf;
	/** track follower helper class */
	typedef NullFollower                      TrackFollower;
	/** node list type */
	typedef Tnode_list                        NodeList;
	typedef Ship                              VehicleType;
	/** pathfinder components (modules) */
	typedef CYapfBaseT<Types>                 PfBase;        // base pathfinder class
	typedef CYapfFollowRegionT<Types>         PfFollow;      // node follower
	typedef CYapfOriginRegionT<Types>         PfOrigin;      // origin provider
	typedef CYapfDestinationRegionT<Types>    PfDestination; // destination/distance provider
	typedef CYapfSegmentCostCacheNoneT<Types> PfCache;       // segment cost cache provider
	typedef CYapfCostRegionT<Types>           PfCost;        // cost provider
};

#endif
