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

#include "embed.hpp"

#include <boost/format.hpp>

#include <alice/rules.hpp>

#include <core/properties.hpp>
#include <core/cli/stores.hpp>

#include <reversible/cli/stores.hpp>
#include <reversible/functions/calculate_additional_lines.hpp>
#include <reversible/synthesis/embed_bdd.hpp>

namespace cirkit
{

/******************************************************************************
 * Types                                                                      *
 ******************************************************************************/

/******************************************************************************
 * Private functions                                                          *
 ******************************************************************************/

/******************************************************************************
 * Public functions                                                           *
 ******************************************************************************/

embed_command::embed_command( const environment::ptr& env )
  : cirkit_command( env, "Embedding" )
{
  opts.add_options()
    ( "bdd,b",      "Embed from BDDs" )
    ( "only_lines", "Only calculate additional lines" )
    ( "new,n",      "Add a new entry to the store; if not set, the current entry is overriden" )
    ;
  be_verbose();
}

command::rules_t embed_command::validity_rules() const
{
  return { has_store_element<bdd_function_t>( env ) };
}

bool embed_command::execute()
{
  const auto& bdds = env->store<bdd_function_t>();
  auto& rcbdds = env->store<rcbdd>();

  const auto settings = make_settings();
  const auto statistics = std::make_shared<properties>();

  if ( is_set( "only_lines" ) )
  {
    auto lines = calculate_additional_lines( bdds.current(), settings, statistics );

    std::cout << boost::format( "[i] required lines: %d" ) % lines << std::endl;
  }
  else
  {
    if ( rcbdds.empty() || is_set( "new" ) )
    {
      rcbdds.extend();
    }

    embed_bdd( rcbdds.current(), bdds.current(), settings, statistics );
  }

  std::cout << boost::format( "[i] run-time: %.2f secs" ) % statistics->get<double>( "runtime" ) << std::endl;

  return true;
}

}

// Local Variables:
// c-basic-offset: 2
// eval: (c-set-offset 'substatement-open 0)
// eval: (c-set-offset 'innamespace 0)
// End:
