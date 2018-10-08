/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2018 James Willis (james.s.willis@durham.ac.uk)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

/* Config parameters. */
#include "../config.h"

/* Some standard headers. */
#include <stddef.h>

/* Local includes. */
#include "swift_velociraptor_part.h"
#include "velociraptor_interface.h"

/* Dummy VELOCIraptor interface for testing compilation without linking the
 * actual VELOCIraptor library. */
#ifdef HAVE_DUMMY_VELOCIRAPTOR
int InitVelociraptor(char *config_name, char *output_name,
                     struct cosmoinfo cosmo_info, struct unitinfo unit_info,
                     struct siminfo sim_info) {
  return 0;
}
int InvokeVelociraptor(const size_t num_gravity_parts,
                       const size_t num_hydro_parts,
                       struct swift_vel_part *swift_parts,
                       const int *cell_node_ids, char *output_name) {
  return 0;
}
#endif /* HAVE_DUMMY_VELOCIRAPTOR */