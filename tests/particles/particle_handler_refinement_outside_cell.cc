// ---------------------------------------------------------------------
//
// Copyright (C) 2017 - 2022 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------



// Check that particles are correctly transferred during grid
// refinement/coarsening. Like particle_handler_05.cc, but with
// a particle that is registered in a parent cell, but will not
// be found in any child cell. Before this fix, this test
// would lead to an endless loop.

#include <deal.II/distributed/tria.h>

#include <deal.II/fe/mapping_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_tools.h>

#include <deal.II/particles/particle_handler.h>

#include "../tests.h"

template <int dim, int spacedim>
void
create_regular_particle_distribution(
  Particles::ParticleHandler<dim, spacedim>                 &particle_handler,
  const parallel::distributed::Triangulation<dim, spacedim> &tr,
  const unsigned int particles_per_direction = 3)
{
  for (unsigned int i = 0; i < particles_per_direction; ++i)
    for (unsigned int j = 0; j < particles_per_direction; ++j)
      {
        Point<spacedim> position;
        Point<dim>      reference_position;
        unsigned int    id = i * particles_per_direction + j;

        position[0] = static_cast<double>(i) /
                      static_cast<double>(particles_per_direction - 1);
        position[1] = static_cast<double>(j) /
                      static_cast<double>(particles_per_direction - 1);

        if (dim > 2)
          for (unsigned int k = 0; k < particles_per_direction; ++k)
            {
              position[2] = static_cast<double>(j) /
                            static_cast<double>(particles_per_direction - 1);
            }
        else
          {
            Particles::Particle<dim, spacedim> particle(position,
                                                        reference_position,
                                                        id);

            typename parallel::distributed::Triangulation<dim, spacedim>::
              active_cell_iterator cell =
                GridTools::find_active_cell_around_point(
                  tr, particle.get_location());

            particle_handler.insert_particle(particle, cell);
          }
      }
}

template <int dim, int spacedim>
void
test()
{
  {
    parallel::distributed::Triangulation<dim, spacedim> tr(MPI_COMM_WORLD);

    GridGenerator::hyper_cube(tr);
    MappingQ<dim, spacedim> mapping(1);

    Particles::ParticleHandler<dim, spacedim> particle_handler(tr, mapping);

    // Create a particle outside of the domain
    Point<spacedim> position;
    position[0] = 3;
    Point<dim> reference_position;
    reference_position[0]    = 0.5;
    types::particle_index id = 0;

    particle_handler.insert_particle(position,
                                     reference_position,
                                     id,
                                     tr.begin_active());

    position[0] = 1.0 + 1e-13;
    id          = 1;

    particle_handler.insert_particle(position,
                                     reference_position,
                                     id,
                                     tr.begin_active());

    for (const auto &particle : particle_handler)
      deallog << "Before refinement particle id " << particle.get_id()
              << " is in cell " << particle.get_surrounding_cell(tr)
              << std::endl;

    // Check that all particles are moved to children
    particle_handler.prepare_for_coarsening_and_refinement();
    tr.refine_global(1);
    particle_handler.unpack_after_coarsening_and_refinement();

    for (const auto &particle : particle_handler)
      deallog << "After refinement particle id " << particle.get_id()
              << " is in cell " << particle.get_surrounding_cell(tr)
              << std::endl;

    // Reverse the refinement and check again
    for (auto cell = tr.begin_active(); cell != tr.end(); ++cell)
      cell->set_coarsen_flag();

    particle_handler.prepare_for_coarsening_and_refinement();
    tr.execute_coarsening_and_refinement();
    particle_handler.unpack_after_coarsening_and_refinement();

    for (const auto &particle : particle_handler)
      deallog << "After coarsening particle id " << particle.get_id()
              << " is in cell " << particle.get_surrounding_cell(tr)
              << std::endl;
  }

  deallog << "OK" << std::endl;
}



int
main(int argc, char *argv[])
{
  Utilities::MPI::MPI_InitFinalize mpi_initialization(argc, argv, 1);

  MPILogInitAll all;

  deallog.push("2d/2d");
  test<2, 2>();
  deallog.pop();
  deallog.push("2d/3d");
  test<2, 3>();
  deallog.pop();
  deallog.push("3d/3d");
  test<3, 3>();
  deallog.pop();
}
