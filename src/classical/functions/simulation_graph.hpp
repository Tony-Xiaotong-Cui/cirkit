/* CirKit: A circuit toolkit
 * Copyright (C) 2009-2015  University of Bremen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file simulation_graph.hpp
 *
 * @brief Simulation graph
 *
 * @author Mathias Soeken
 * @since  2.0
 */

#ifndef SIMULATION_GRAPH_HPP
#define SIMULATION_GRAPH_HPP

#include <unordered_map>
#include <vector>

#include <boost/dynamic_bitset.hpp>
#include <boost/functional/hash.hpp>
#include <boost/graph/adjacency_list.hpp>

#include <igraph/igraph.h>

#include <core/properties.hpp>
#include <core/utils/graph_utils.hpp>
#include <classical/aig.hpp>

namespace boost
{

enum graph_edge_lookup_t { graph_edge_lookup };
BOOST_INSTALL_PROPERTY(graph, edge_lookup);

enum vertex_support_t { vertex_support };
BOOST_INSTALL_PROPERTY(vertex, support);

}

namespace cirkit
{

using vertex_pair_t    = std::pair<unsigned, unsigned>;

struct edge_lookup_hash_t
{
  inline std::size_t operator()( const vertex_pair_t& p ) const
  {
    std::size_t seed = 0;
    boost::hash_combine( seed, p.first );
    boost::hash_combine( seed, p.second );
    return seed;
  }
};

namespace detail
{
  using simulation_graph_traits_t          = boost::adjacency_list_traits<boost::vecS, boost::vecS, boost::undirectedS>;
}

using edge_lookup_t                        = std::unordered_map<vertex_pair_t, detail::simulation_graph_traits_t::edge_descriptor, edge_lookup_hash_t>;

/* In order to allow an O(1) lookup for edges in the graph, this
 * edge lookup table is added as a property to the simulation graph
 */
using simulation_graph_properties_t        = boost::property<boost::graph_edge_lookup_t, edge_lookup_t>;

/* In order to allow an O(1) access to the vertex in- and out-degree,
 * they are stored additionally as property maps for the vertices.
 */
using simulation_graph_vertex_properties_t = boost::property<boost::vertex_in_degree_t, unsigned,
                                             boost::property<boost::vertex_out_degree_t, unsigned,
                                             boost::property<boost::vertex_support_t, unsigned,
                                             boost::property<boost::vertex_name_t, unsigned>>>>;

using simulation_graph_edge_properties_t   = boost::property<boost::edge_name_t, unsigned>;

using simulation_graph                     = graph_t<simulation_graph_vertex_properties_t,
                                                     simulation_graph_edge_properties_t,
                                                     simulation_graph_properties_t>;
using simulation_node                      = vertex_t<simulation_graph>;
using simulation_edge                      = edge_t<simulation_graph>;

enum class simulation_pattern : unsigned
{
  all_hot = 0x1,
  one_hot = 0x2,
  two_hot = 0x4,
  all_cold = 0x8,
  one_cold = 0x10,
  two_cold = 0x20
};

/**
 * This function does not assign labels
 */
simulation_graph create_simulation_graph( const aig_graph& aig, const std::vector<boost::dynamic_bitset<>>& sim_vectors,
                                          const properties::ptr& settings = properties::ptr(),
                                          const properties::ptr& statistics = properties::ptr() );

/**
 * @param partition If not nullptr then the number of patterns for each selector is written into it.
 */
std::vector<boost::dynamic_bitset<>> create_simulation_vectors( unsigned width, unsigned selector,
                                                                std::vector<unsigned>* partition = nullptr );

simulation_graph create_simulation_graph( const aig_graph& aig, unsigned selector,
                                          const properties::ptr& settings = properties::ptr(),
                                          const properties::ptr& statistics = properties::ptr() );

igraph_t simulation_graph_to_igraph( const simulation_graph& g );

}

#endif

// Local Variables:
// c-basic-offset: 2
// eval: (c-set-offset 'substatement-open 0)
// eval: (c-set-offset 'innamespace 0)
// End:
