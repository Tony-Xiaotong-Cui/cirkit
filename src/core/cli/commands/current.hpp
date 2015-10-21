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
 * @file current.hpp
 *
 * @brief Switches current data structure
 *
 * @author Mathias Soeken
 * @since  2.3
 */

#ifndef CLI_CURRENT_COMMAND_HPP
#define CLI_CURRENT_COMMAND_HPP

#include <core/cli/command.hpp>
#include <core/cli/environment.hpp>
#include <core/cli/store.hpp>
#include <core/utils/program_options.hpp>

using namespace boost::program_options;

namespace cirkit
{

template<typename T>
bool exactly_one_true_helper( std::initializer_list<T> list )
{
  auto current = false;

  for ( auto i : list )
  {
    if ( i )
    {
      if ( current ) { return false; }
      current = true;
    }
  }

  return current;
}

template<typename S>
int set_current_index_helper( const program_options& opts, const environment::ptr& env, unsigned index )
{
  constexpr auto option = store_info<S>::option;

  if ( opts.is_set( option ) && index < env->store<S>().size() )
  {
    env->store<S>().set_current_index( index );
  }
  return 0;
}

template<class... S>
class current_command : public command
{
public:
  current_command( const environment::ptr& env )
    : command( env, "Switches current data structure" )
  {
    opts.set_positional_option( "index" );
    opts.add_options()
      ( "index,i", value( &index ), "New index" )
      ;

    [](...){}( add_option_helper<S>( opts )... );
  }

protected:
  rules_t validity_rules() const
  {
    rules_t rules;

    rules.push_back( {[&]() { return exactly_one_true_helper( { opts.is_set( store_info<S>::option )... } ); }, "exactly one store needs to be specified" } );

    return rules;
  }

  bool execute()
  {
    [](...){}( set_current_index_helper<S>( opts, env, index )... );

    return true;
  }

private:
  unsigned index;
};

}

#endif

// Local Variables:
// c-basic-offset: 2
// eval: (c-set-offset 'substatement-open 0)
// eval: (c-set-offset 'innamespace 0)
// End:
