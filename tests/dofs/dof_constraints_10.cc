// ---------------------------------------------------------------------
//
// Copyright (C) 2004 - 2022 by the deal.II authors
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



// simply check what happens when condensing compressed sparsity
// patterns. This test was written when I changed a few things in the
// algorithm.

#include <deal.II/dofs/dof_accessor.h>
#include <deal.II/dofs/dof_handler.h>
#include <deal.II/dofs/dof_tools.h>

#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_tools.h>
#include <deal.II/grid/tria.h>
#include <deal.II/grid/tria_accessor.h>
#include <deal.II/grid/tria_iterator.h>

#include <deal.II/lac/affine_constraints.h>
#include <deal.II/lac/dynamic_sparsity_pattern.h>
#include <deal.II/lac/sparsity_pattern.h>

#include "../tests.h"


template <int dim>
void
test()
{
  deallog << dim << 'D' << std::endl;

  Triangulation<dim> triangulation;
  GridGenerator::hyper_cube(triangulation);

  // refine once, then refine first cell to
  // create hanging nodes
  triangulation.refine_global(1);
  triangulation.begin_active()->set_refine_flag();
  triangulation.execute_coarsening_and_refinement();
  deallog << "Number of cells: " << triangulation.n_active_cells() << std::endl;

  // set up a DoFHandler and compute hanging
  // node constraints for a Q2 element
  FE_Q<dim>       fe(2);
  DoFHandler<dim> dof_handler(triangulation);
  dof_handler.distribute_dofs(fe);
  deallog << "Number of dofs: " << dof_handler.n_dofs() << std::endl;

  AffineConstraints<double> constraints;
  DoFTools::make_hanging_node_constraints(dof_handler, constraints);
  constraints.close();
  deallog << "Number of constraints: " << constraints.n_constraints()
          << std::endl;

  // then set up a sparsity pattern and a
  // matrix on top of it
  DynamicSparsityPattern sparsity(dof_handler.n_dofs(), dof_handler.n_dofs());
  DoFTools::make_sparsity_pattern(dof_handler, sparsity);
  constraints.condense(sparsity);

  // and output what we have
  SparsityPattern A;
  A.copy_from(sparsity);
  for (SparsityPattern::const_iterator i = A.begin(); i != A.end(); ++i)
    deallog << i->row() << ' ' << i->column() << std::endl;
}



int
main()
{
  initlog();

  try
    {
      test<1>();
      test<2>();
      test<3>();
    }
  catch (const std::exception &exc)
    {
      deallog << std::endl
              << std::endl
              << "----------------------------------------------------"
              << std::endl;
      deallog << "Exception on processing: " << std::endl
              << exc.what() << std::endl
              << "Aborting!" << std::endl
              << "----------------------------------------------------"
              << std::endl;

      return 1;
    }
  catch (...)
    {
      deallog << std::endl
              << std::endl
              << "----------------------------------------------------"
              << std::endl;
      deallog << "Unknown exception!" << std::endl
              << "Aborting!" << std::endl
              << "----------------------------------------------------"
              << std::endl;
      return 1;
    };
}
