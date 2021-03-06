/* CirKit: A circuit toolkit
 * Copyright (C) 2009-2015  University of Bremen
 * Copyright (C) 2015-2016  EPFL
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "write_qpic.hpp"

#include <fstream>
#include <vector>

#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>

#include <reversible/target_tags.hpp>

namespace cirkit
{

/******************************************************************************
 * Types                                                                      *
 ******************************************************************************/

/******************************************************************************
 * Private functions                                                          *
 ******************************************************************************/

std::string format_control( const variable& v )
{
  return boost::str( boost::format( "%sl%d" ) % ( v.polarity() ? "" : "-" ) % v.line() );
}

std::string format_target( unsigned t, const std::string& prefix = std::string() )
{
  return boost::str( boost::format( boost::format( "%sl%d" ) % prefix % t ) );
}

void write_qpic( const circuit& circ, std::ostream& os, const properties::ptr& settings )
{
  const auto print_index = get( settings, "print_index", false );

  for ( auto i = 0u; i < circ.lines(); ++i )
  {
    os << boost::format( "l%d W %s %s" ) % i % circ.inputs()[i] % circ.outputs()[i] << std::endl;
  }

  for ( auto i = 0u; i < circ.num_gates(); ++i )
  {
    const auto& g = circ[i];
    std::vector<std::string> items;

    if ( is_toffoli( g ) )
    {
      for ( const auto& c : g.controls() )
      {
        items.push_back( format_control( c ) );
      }

      for ( const auto& t : g.targets() )
      {
        items.push_back( format_target( t, "+" ) );
      }
    }
    else if ( is_fredkin( g ) )
    {
      assert( g.targets().size() == 2u );

      for ( const auto& t : g.targets() )
      {
        items.push_back( format_target( t ) );
      }
      items.push_back( "SWAP" );
      for ( const auto& c : g.controls() )
      {
        items.push_back( format_control( c ) );
      }
    }
    else
    {
      assert( false );
    }

    os << boost::join( items, " " );
    if ( print_index )
    {
      os << " %%" << i;
    }
    os << std::endl;
  }
}

/******************************************************************************
 * Public functions                                                           *
 ******************************************************************************/

void write_qpic( const circuit& circ, const std::string& filename, const properties::ptr& settings )
{
  std::ofstream os( filename.c_str(), std::ofstream::out );
  write_qpic( circ, os, settings );
}

}

// Local Variables:
// c-basic-offset: 2
// eval: (c-set-offset 'substatement-open 0)
// eval: (c-set-offset 'innamespace 0)
// End:
