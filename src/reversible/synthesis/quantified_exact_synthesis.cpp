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

#include "quantified_exact_synthesis.hpp"

#include <cmath>
#include <vector>

#include <boost/assign/std/vector.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/pending/integer_log2.hpp>
#include <boost/range/combine.hpp>
#include <boost/range/numeric.hpp>

#include <core/utils/bdd_utils.hpp>
#include <core/utils/range_utils.hpp>
#include <core/utils/timer.hpp>
#include <reversible/gate.hpp>
#include <reversible/functions/add_gates.hpp>
#include <reversible/functions/copy_metadata.hpp>

#include <cuddObj.hh>

using namespace boost::assign;

namespace cirkit
{

/******************************************************************************
 * Private functions                                                          *
 ******************************************************************************/
inline std::vector<BDD> create_variables( Cudd& manager, unsigned count )
{
  return generate_vector<BDD>( count, [&manager]() { return manager.bddVar(); } );
}

std::vector<BDD> perform_gate( Cudd& manager, const std::vector<BDD>& inputs, const std::vector<BDD>& gate, bool negative )
{
  const auto n = inputs.size();

  /* Match? */
  BDD match = manager.bddOne();
  for ( auto input : index( inputs ) )
  {
    const auto pos_ctrl = gate[input.index];
    if ( negative )
    {
      const auto neg_ctrl = gate[n + input.index];
      match &= ( !pos_ctrl | input.value ) & ( !neg_ctrl | !input.value );
    }
    else
    {
      match &= ( !pos_ctrl | input.value );
    }
  }

  /* Outputs */
  std::vector<BDD> outputs;
  const auto offset = negative ? ( n << 1u ) : n;

  for ( auto i = 0u; i < n; ++i )
  {
    boost::dynamic_bitset<> target( gate.size() - offset, i );
    BDD is_target = manager.bddOne();
    for ( unsigned j = 0u; j < target.size(); ++j )
    {
      is_target &= target[j] ? gate[offset + j] : !gate[offset + j];
    }

    outputs += inputs[i] ^ ( match & is_target );
  }

  return outputs;
}

std::vector<BDD> bdd_for_function( Cudd& manager, const binary_truth_table& spec, const std::vector<BDD>& inputs )
{
  unsigned n = spec.num_inputs();
  std::vector<BDD> result( n, manager.bddZero() );

  for ( const auto& row : spec )
  {
    BDD cube = manager.bddOne();
    unsigned index = 0u;
    for ( const auto& x : boost::make_iterator_range( row.first ) )
    {
      if ( (bool)x )
      {
        cube &= *x ? inputs[index] : !inputs[index];
      }
      ++index;
    }

    index = 0u;
    for ( const auto& y : boost::make_iterator_range( row.second ) )
    {
      if ( (bool)y && *y )
      {
        result[index] |= cube;
      }
      ++index;
    }
  }

  return result;
}

inline BDD create_result_function( Cudd& manager, const std::vector<BDD>& circ, const std::vector<BDD>& function )
{
  return boost::accumulate( boost::combine( circ, function ),
                            manager.bddOne(),
                            []( BDD current, const boost::tuple<BDD, BDD>& t ) {
                              return current & ( boost::get<0>( t ).Xnor( boost::get<1>( t ) ) );
                            } );
}

template<typename T>
void extract_solution( circuit& circ, T * str, unsigned num_gates, unsigned n, unsigned nbits, bool negative, bool verbose )
{
  if ( verbose )
  {
    std::cout << "[i] extract solution for " << num_gates << " gates." << std::endl;
  }

  const auto cbits = negative ? ( n << 1u ) : n;

  for ( auto i = 0u; i < num_gates; ++i )
  {
    auto offset = n + i * ( cbits + nbits );

    /* controls */
    gate::control_container controls;
    for ( auto j = 0u; j < n; ++j )
    {
      if ( str[offset + j] == 1 )
      {
        controls += make_var( j );
        assert( negative || str[offset + n + j] != 1 );
      }

      if ( negative && str[offset + n + j] == 1 )
      {
        controls += make_var( j, false );
      }
    }

    /* target */
    offset += cbits;
    boost::dynamic_bitset<> target( nbits );
    for ( unsigned j = 0u; j < nbits; ++j )
    {
      target.set( j, str[offset + j] == 1 );
    }

    append_toffoli( circ, controls, target.to_ulong() );
  }
}

/******************************************************************************
 * Public functions                                                           *
 ******************************************************************************/

bool quantified_exact_synthesis( circuit& circ, const binary_truth_table& spec, properties::ptr settings, properties::ptr statistics )
{
  const auto max_depth     = get( settings, "max_depth",     20u );
  const auto negative      = get( settings, "negative",      false );
  const auto all_solutions = get( settings, "all_solutions", false );
  const auto verbose       = get( settings, "verbose",       false );

  properties_timer t( statistics );

  const auto n = spec.num_inputs();
  const auto nbits = (unsigned)ceil( log( n ) / log( 2.0) );
  const auto cbits = negative ? ( n << 1u ) : n;

  circ.set_lines( n );

  Cudd manager;

  /* input variables and gate variables */
  const auto xs = create_variables( manager, n );
  const BDD xscube = make_cube( manager, xs );

  /* specification */
  const auto function = bdd_for_function( manager, spec, xs );

  /* find circuit */
  auto current    = xs;
  auto gate_count = 0u;
  auto result     = false;

  do
  {
    if ( verbose )
    {
      std::cout << "[i] check for depth " << gate_count << std::endl;
    }

    auto f = create_result_function( manager, current, function ).UnivAbstract( xscube );

    if ( f != manager.bddZero() )
    {
      const auto num_vars = n + gate_count * ( cbits + nbits );
      char * str = new char[num_vars];
      f.PickOneCube( str );
      extract_solution( circ, str, gate_count, n, nbits, negative, verbose );
      delete str;

      set( statistics, "num_circuits", (unsigned)( f.CountMinterm( num_vars ) / ( 1u << n ) ) );

      if ( all_solutions )
      {
        std::vector<circuit> solutions;

        DdGen *gen;
        CUDD_VALUE_TYPE value;
        int * cube;
        Cudd_ForeachCube( manager.getManager(), f.getNode(), gen, cube, value )
        {
          circuit sol_circ( n );
          copy_metadata( spec, sol_circ );
          extract_solution( sol_circ, cube, gate_count, n, nbits, negative, verbose );
          solutions += sol_circ;
        }

        set( statistics, "solutions", solutions );
      }

      result = true;
    }
    else
    {
      /* extend circuit */
      auto gate = create_variables( manager, cbits + nbits );
      current = perform_gate( manager, current, gate, negative );
    }
  } while ( !result && ++gate_count < max_depth );

  if ( result )
  {
    copy_metadata( spec, circ );
  }
  else
  {
    set_error_message( statistics,
        "Could not find a circuit within the predefined depth." );
  }

  return result;
}

truth_table_synthesis_func quantified_exact_synthesis_func( properties::ptr settings, properties::ptr statistics )
{
  truth_table_synthesis_func f = [&settings, &statistics]( circuit& circ, const binary_truth_table& spec ) {
    return quantified_exact_synthesis( circ, spec, settings, statistics );
  };
  f.init( settings, statistics );
  return f;
}

}

// Local Variables:
// c-basic-offset: 2
// eval: (c-set-offset 'substatement-open 0)
// eval: (c-set-offset 'innamespace 0)
// End:
