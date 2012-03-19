/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file yapf_ship.cpp Implementation of YAPF for ships. */

#include "../../stdafx.h"
#include "../../ship.h"

#include "yapf.hpp"
#include "yapf_node_ship.hpp"
#include "region_common.h"
#include "region_manager.hpp"
#include "region_d_water.h"

/** Node Follower module of YAPF for ships */
template <class Types>
class CYapfFollowShipT
{
public:
	typedef typename Types::Tpf Tpf;                     ///< the pathfinder class (derived from THIS class)
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node;        ///< this will be our node type
	typedef typename Node::Key Key;                      ///< key to hash tables

protected:
	/** to access inherited path finder */
	inline Tpf& Yapf()
	{
		return *static_cast<Tpf*>(this);
	}

public:
	/**
	 * Called by YAPF to move from the given node to the next tile. For each
	 *  reachable trackdir on the new tile creates new node, initializes it
	 *  and adds it to the open list by calling Yapf().AddNewNode(n)
	 */
	inline void PfFollowNode(Node& old_node)
	{
		TrackFollower F(Yapf().GetVehicle());
		if (F.Follow(old_node.m_key.m_tile, old_node.m_key.m_td)) {
			Yapf().AddMultipleNodes(&old_node, F);
		}
	}

	/** return debug report character to identify the transportation type */
	inline char TransportTypeChar() const
	{
		return 'w';
	}

	static Trackdir ChooseShipTrack(const Ship *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool &path_found)
	{
		/* handle special case - when next tile is destination tile */
		if (tile == v->dest_tile) {
			/* convert tracks to trackdirs */
			TrackdirBits trackdirs = (TrackdirBits)(tracks | ((int)tracks << 8));
			/* choose any trackdir reachable from enterdir */
			trackdirs &= DiagdirReachesTrackdirs(enterdir);
			return (Trackdir)FindFirstBit2x64(trackdirs);
		}

		/* move back to the old tile/trackdir (where ship is coming from) */
		TileIndex src_tile = TILE_ADD(tile, TileOffsByDiagDir(ReverseDiagDir(enterdir)));
		Trackdir trackdir = v->GetVehicleTrackdir();
		assert(IsValidTrackdir(trackdir));

		/* convert origin trackdir to TrackdirBits */
		TrackdirBits trackdirs = TrackdirToTrackdirBits(trackdir);

		/* Use the region route finder to get a destination (either final or intermediate) */
		/* regions_ahead tells the region finder how many regions ahead we wish to look */
		uint regions_ahead = 2;
		bool region_path_not_found = false;
		vector<TileIndex> route_tiles = YapfRegionWater(v, src_tile,v->dest_tile,regions_ahead,&region_path_not_found);
		if (_debug_yapf_level >= 3) {
			show_route_tiles = set<TileIndex>(route_tiles.begin(), route_tiles.end());
			MarkWholeScreenDirty();
		}
		if (region_path_not_found){
			path_found = false;
			return INVALID_TRACKDIR;
		}
		bool temp_path_found = false;
		Node *pNode = NULL;
		Tpf *pf = NULL;

		/*Try to go to the region furthest ahead (out of the ones we asked for, not of all the regions on the route)
		  Almost always we can go to the furthest one, but the user might have lowerd the YAPF node limit
		  or we might be in a really twisty path */
		for (uint iDest = 0; !temp_path_found && iDest < route_tiles.size(); ++iDest){
			/* create pathfinder instance */
			delete pf;
			pf = new Tpf();
			/* get available trackdirs on the destination tile */
			TrackdirBits dest_trackdirs = (TrackdirBits)(GetTileTrackStatus(route_tiles[iDest], TRANSPORT_WATER, 0) & TRACKDIR_BIT_MASK);
			/* set origin and destination nodes */
			pf->SetOrigin(src_tile, trackdirs);
			pf->SetDestination(route_tiles[iDest], dest_trackdirs);
			/* find best path */
			temp_path_found = pf->FindPath(v);
			pNode = pf->GetBestNode();
		}
		
		
		if (!temp_path_found) {
				/* tell controller that the path was only 'guessed' */
				path_found = false;
		}

		Trackdir next_trackdir = INVALID_TRACKDIR; /* this would mean "path not found" */

		if (pNode != NULL) {
			/* walk through the path back to the origin */
			Node *pPrevNode = NULL;
			while (pNode->m_parent != NULL) {
				pPrevNode = pNode;
				pNode = pNode->m_parent;
			}
			/* return trackdir from the best next node (direct child of origin) */
			Node& best_next_node = *pPrevNode;
			assert(best_next_node.GetTile() == tile);
			next_trackdir = best_next_node.GetTrackdir();
		}
		delete pf;
		return next_trackdir;
	}
};

/** Cost Provider module of YAPF for ships */
template <class Types>
class CYapfCostShipT
{
public:
	typedef typename Types::Tpf Tpf;              ///< the pathfinder class (derived from THIS class)
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList::Titem Node; ///< this will be our node type
	typedef typename Node::Key Key;               ///< key to hash tables

protected:
	/** to access inherited path finder */
	Tpf& Yapf()
	{
		return *static_cast<Tpf*>(this);
	}

public:
	/**
	 * Called by YAPF to calculate the cost from the origin to the given node.
	 *  Calculates only the cost of given node, adds it to the parent node cost
	 *  and stores the result into Node::m_cost member
	 */
	inline bool PfCalcCost(Node& n, const TrackFollower *tf)
	{
		/* base tile cost depending on distance */
		int c = IsDiagonalTrackdir(n.GetTrackdir()) ? YAPF_TILE_LENGTH : YAPF_TILE_CORNER_LENGTH;
		/* additional penalty for curves */
		if (n.m_parent != NULL && n.GetTrackdir() != NextTrackdir(n.m_parent->GetTrackdir())) {
			/* new trackdir does not match the next one when going straight */
			c += YAPF_TILE_LENGTH;
		}

		/*penalty for being near shore when at sea (gives ships more realistic routes) */
		if (GetEffectiveWaterClass(n.GetTile()) == WATER_CLASS_SEA) {
			uint edge_count = 0;
			uint x = TileX(n.GetTile());
			uint y = TileY(n.GetTile());
			for (short i = 1; i<4; ++i)
			{
				edge_count += (RegionDescriptionWater::IsRoutable(TileXY(x+i,y)) ? 0 : 1);
				edge_count += (RegionDescriptionWater::IsRoutable(TileXY(x-i,y)) ? 0 : 1);
				edge_count += (RegionDescriptionWater::IsRoutable(TileXY(x,y+i)) ? 0 : 1);
				edge_count += (RegionDescriptionWater::IsRoutable(TileXY(x,y-i)) ? 0 : 1);
			}
			/*Penalise if only one edge is land - if 2 or more then we risk making the ship not use a small gap
			 which would then lead to it looping as the regions would say the gap was there*/
			 c += (edge_count == 1 ? YAPF_TILE_LENGTH : 0);
		}

		
		/* Skipped tile cost for aqueducts. */
		c += YAPF_TILE_LENGTH * tf->m_tiles_skipped;

		/* Ocean/canal speed penalty. */
		const ShipVehicleInfo *svi = ShipVehInfo(Yapf().GetVehicle()->engine_type);
		byte speed_frac = (GetEffectiveWaterClass(n.GetTile()) == WATER_CLASS_SEA) ? svi->ocean_speed_frac : svi->canal_speed_frac;
		if (speed_frac > 0) c += YAPF_TILE_LENGTH * (1 + tf->m_tiles_skipped) * speed_frac / (256 - speed_frac);

		/* apply it */
		n.m_cost = n.m_parent->m_cost + c;
		return true;
	}
};

/**
 * Config struct of YAPF for ships.
 *  Defines all 6 base YAPF modules as classes providing services for CYapfBaseT.
 */
template <class Tpf_, class Ttrack_follower, class Tnode_list>
struct CYapfShip_TypesT
{
	/** Types - shortcut for this struct type */
	typedef CYapfShip_TypesT<Tpf_, Ttrack_follower, Tnode_list>  Types;

	/** Tpf - pathfinder type */
	typedef Tpf_                              Tpf;
	/** track follower helper class */
	typedef Ttrack_follower                   TrackFollower;
	/** node list type */
	typedef Tnode_list                        NodeList;
	typedef Ship                              VehicleType;
	/** pathfinder components (modules) */
	typedef CYapfBaseT<Types>                 PfBase;        // base pathfinder class
	typedef CYapfFollowShipT<Types>           PfFollow;      // node follower
	typedef CYapfOriginTileT<Types>           PfOrigin;      // origin provider
	typedef CYapfDestinationTileT<Types>      PfDestination; // destination/distance provider
	typedef CYapfSegmentCostCacheNoneT<Types> PfCache;       // segment cost cache provider
	typedef CYapfCostShipT<Types>             PfCost;        // cost provider
};

/* YAPF type 1 - uses TileIndex/Trackdir as Node key, allows 90-deg turns */
struct CYapfShip1 : CYapfT<CYapfShip_TypesT<CYapfShip1, CFollowTrackWater    , CShipNodeListTrackDir> > {};
/* YAPF type 2 - uses TileIndex/DiagDirection as Node key, allows 90-deg turns */
struct CYapfShip2 : CYapfT<CYapfShip_TypesT<CYapfShip2, CFollowTrackWater    , CShipNodeListExitDir > > {};
/* YAPF type 3 - uses TileIndex/Trackdir as Node key, forbids 90-deg turns */
struct CYapfShip3 : CYapfT<CYapfShip_TypesT<CYapfShip3, CFollowTrackWaterNo90, CShipNodeListTrackDir> > {};

/** Ship controller helper - path finder invoker */
Track YapfShipChooseTrack(const Ship *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool &path_found)
{
	/* default is YAPF type 2 */
	typedef Trackdir (*PfnChooseShipTrack)(const Ship*, TileIndex, DiagDirection, TrackBits, bool &path_found);
	PfnChooseShipTrack pfnChooseShipTrack = CYapfShip2::ChooseShipTrack; // default: ExitDir, allow 90-deg

	/* check if non-default YAPF type needed */
	if (_settings_game.pf.forbid_90_deg) {
		pfnChooseShipTrack = &CYapfShip3::ChooseShipTrack; // Trackdir, forbid 90-deg
	} else if (_settings_game.pf.yapf.disable_node_optimization) {
		pfnChooseShipTrack = &CYapfShip1::ChooseShipTrack; // Trackdir, allow 90-deg
	}

	Trackdir td_ret = pfnChooseShipTrack(v, tile, enterdir, tracks, path_found);
	return (td_ret != INVALID_TRACKDIR) ? TrackdirToTrack(td_ret) : INVALID_TRACK;
}
