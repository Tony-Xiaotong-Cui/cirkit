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

#include "exorcism_minimization.hpp"

#include <fcntl.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include <core/io/pla_parser.hpp>
#include <core/utils/bdd_utils.hpp>
#include <core/utils/system_utils.hpp>
#include <core/utils/timer.hpp>
#include <classical/abc/abc_api.hpp>
#include <classical/abc/functions/cirkit_to_gia.hpp>

#include <misc/vec/vecInt.h>
#include <misc/vec/vecWec.h>

namespace abc
{
int Abc_ExorcismMain( Vec_Wec_t * vEsop, int nIns, int nOuts, char * pFileNameOut, int Quality, int Verbosity, int nCubesMax, int fUseQCost );
Gia_Man_t * Eso_ManCompute( Gia_Man_t * pGia, int fVerbose, Vec_Wec_t ** pvRes );
}

namespace cirkit
{

/******************************************************************************
 * Private functions                                                          *
 ******************************************************************************/

class exorcism_processor : public pla_processor
{
public:
  exorcism_processor( const cube_function_t& on_cube_f ) : on_cube_f( on_cube_f ) {}

  void on_cube( const std::string& in, const std::string& out )
  {
    /* only one output functions are supported */
    assert( out.size() == 1u );

    unsigned n = in.size();
    boost::dynamic_bitset<> values( n );
    boost::dynamic_bitset<> care( n );

    for ( unsigned i = 0u; i < n; ++i )
    {
      values[i] = ( in[i] == '1' );
      care[i]   = ( in[i] != '-' );
    }

    cube_t cube = std::make_pair( values, care );

    if ( on_cube_f )
    {
      on_cube_f( cube );
    }

    ++_cube_count;
    _literal_count += care.count();
  }

  unsigned cube_count() const
  {
    return _cube_count;
  }

  unsigned literal_count() const
  {
    return _literal_count;
  }

private:
  const cube_function_t& on_cube_f;
  unsigned _cube_count = 0u;
  unsigned _literal_count = 0u;
};

/******************************************************************************
 * Public functions                                                           *
 ******************************************************************************/

void exorcism_minimization( DdManager * cudd, DdNode * f, const properties::ptr& settings, const properties::ptr& statistics )
{
  return exorcism_minimization( bdd_to_cubes( cudd, f ), settings, statistics );

  // /* Settings */
  // std::string tmpfile = get( settings, "tmpfile", std::string( "/tmp/test.pla" ) );

  // /* Re-route stdout of Cudd */
  // FILE * old = Cudd_ReadStdout( cudd );

  // /* Print cover to file */
  // FILE * fd = fopen( tmpfile.c_str(), "w" );
  // Cudd_SetStdout( cudd, fd );

  // fprintf( fd, ".i %d\n.o 1\n", Cudd_ReadSize( cudd ) );
  // Cudd_PrintMinterm( cudd, f );
  // fprintf( fd, ".e\n" );

  // fclose( fd );

  // /* Re-set stdout of Cudd */
  // Cudd_SetStdout( cudd, old );

  // /* Run exorcism based on the file */
  // exorcism_minimization( tmpfile, settings, statistics );
}

void exorcism_minimization( const std::string& filename, const properties::ptr& settings, const properties::ptr& statistics )
{
  /* Settings */
  bool            verbose  = get( settings, "verbose",  false                     );
  std::string     exorcism = get( settings, "exorcism", std::string( "exorcism" ) );
  cube_function_t on_cube  = get( settings, "on_cube",  cube_function_t()         );


  /* Call */
  std::string hide_output = verbose ? "" : " > /dev/null 2>&1";
  system( boost::str( boost::format( "(%s %s%s; echo > /dev/null)" ) % exorcism % filename % hide_output ).c_str() );

  /* Get ESOP filename */
  boost::filesystem::path path( filename );
  std::string esopname = boost::str( boost::format( "%s/%s.esop" ) % path.parent_path().string() % path.stem().string() );

  /* Parse */
  exorcism_processor p( on_cube );
  pla_parser( esopname, p );

  if ( statistics )
  {
    statistics->set( "cube_count", p.cube_count() );
    statistics->set( "literal_count", p.literal_count() );
  }
}

void exorcism_minimization( const cube_vec_t& cubes,
                            const properties::ptr& settings,
                            const properties::ptr& statistics )
{
  const auto verbose      = get( settings, "verbose",      false );
  const auto on_cube      = get( settings, "on_cube",      cube_function_t() );
  const auto esopname     = get( settings, "esopname",     std::string( "/tmp/test.esop" ) );
  const auto skip_parsing = get( settings, "skip_parsing", false );

  if ( cubes.empty() )
  {
    return;
  }

  abc::Vec_Wec_t *esop = abc::Vec_WecAlloc( 0u );

  for ( const auto& cube : cubes )
  {
    auto * level = abc::Vec_WecPushLevel( esop );

    for ( auto i = 0u; i < cube.length(); ++i )
    {
      if ( !cube.care()[i] ) { continue; }

      abc::Vec_IntPush( level, ( i << 1u ) | !cube.bits()[i] );
    }
    abc::Vec_IntPush( level, -1 );
  }

  if ( verbose )
  {
    abc::Vec_WecPrint( esop, 0 );
  }

  /* redict STDOUT because of one output line in Abc_ExorcismMain */

  int p_bak, p_new;
  fflush( stdout );
  p_bak = dup( 1 );
  p_new = open( "/dev/null", O_WRONLY );
  dup2( p_new, 1 );
  close( p_new );
  abc::Abc_ExorcismMain( esop, cubes.front().length(), 1, const_cast<char*>( esopname.c_str() ), 2, verbose, 20000, 0 );
  fflush( stdout );
  dup2( p_bak, 1 );
  close( p_bak );

  abc::Vec_WecFree( esop );

  /* Parse */
  if ( !skip_parsing )
  {
    exorcism_processor p( on_cube );
    pla_parser( esopname, p );

    set( statistics, "cube_count", p.cube_count() );
    set( statistics, "literal_count", p.literal_count() );
  }
}

void exorcism_minimization( const aig_graph& aig, const properties::ptr& settings, const properties::ptr& statistics )
{
  const auto verbose      = get( settings, "verbose",      false );
  const auto on_cube      = get( settings, "on_cube",      cube_function_t() );
  const auto esopname     = get( settings, "esopname",     std::string( "/tmp/test.esop" ) );
  const auto skip_parsing = get( settings, "skip_parsing", false );

  abc::Vec_Wec_t * esop = nullptr;
  auto* gia = cirkit_to_gia( aig );

  abc::Eso_ManCompute( gia, verbose, &esop );
  abc::Abc_ExorcismMain( esop, abc::Gia_ManCiNum( gia ), abc::Gia_ManCoNum( gia ), const_cast<char*>( esopname.c_str() ), 2, verbose, 20000, 0 );
  abc::Vec_WecFree( esop );

  /* Parse */
  if ( !skip_parsing )
  {
    exorcism_processor p( on_cube );
    pla_parser( esopname, p );

    set( statistics, "cube_count", p.cube_count() );
    set( statistics, "literal_count", p.literal_count() );
  }
}

void exorcism_minimization_blif( const std::string& filename, const properties::ptr& settings, const properties::ptr& statistics )
{
  const auto verbose      = get( settings, "verbose",      false );
  const auto on_cube      = get( settings, "on_cube",      cube_function_t() );
  const auto esopname     = get( settings, "esopname",     std::string( "/tmp/test.esop" ) );
  const auto skip_parsing = get( settings, "skip_parsing", false );

  properties_timer t( statistics );

  execute_and_omit( boost::str( boost::format( "abc -c \"read_blif %s; strash; &get; &exorcism %s\"" ) % filename % esopname ) );

  /* Parse */
  if ( !skip_parsing )
  {
    exorcism_processor p( on_cube );
    pla_parser( esopname, p );

    set( statistics, "cube_count", p.cube_count() );
    set( statistics, "literal_count", p.literal_count() );
  }
}

dd_based_esop_optimization_func dd_based_exorcism_minimization_func(properties::ptr settings, properties::ptr statistics)
{
  dd_based_esop_optimization_func f = [&settings, &statistics]( DdManager * cudd, DdNode * f ) {
    return exorcism_minimization( cudd, f, settings, statistics );
  };
  f.init( settings, statistics );
  return f;
}

pla_based_esop_optimization_func pla_based_exorcism_minimization_func(properties::ptr settings, properties::ptr statistics)
{
  pla_based_esop_optimization_func f = [&settings, &statistics]( const std::string& filename ) {
    return exorcism_minimization( filename, settings, statistics );
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
