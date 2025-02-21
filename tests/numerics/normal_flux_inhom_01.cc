// ---------------------------------------------------------------------
//
// Copyright (C) 2007 - 2020 by the deal.II authors
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



// check the creation of inhomogeneous normal-flux boundary conditions
// for a finite element that consists of only a single set of vector
// components (i.e. it has dim components)

#include <deal.II/base/function.h>
#include <deal.II/base/quadrature_lib.h>

#include <deal.II/dofs/dof_handler.h>

#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_system.h>
#include <deal.II/fe/fe_values.h>
#include <deal.II/fe/mapping_q1.h>

#include <deal.II/grid/grid_generator.h>

#include <deal.II/lac/affine_constraints.h>
#include <deal.II/lac/vector.h>

#include <deal.II/numerics/vector_tools.h>

#include "../tests.h"



template <int dim>
void
test(const Triangulation<dim> &tr, const FiniteElement<dim> &fe)
{
  DoFHandler<dim> dof(tr);
  dof.distribute_dofs(fe);

  Functions::ConstantFunction<dim> constant_function(1., dim);
  std::map<types::boundary_id, const Function<dim> *> function_map;
  for (const unsigned int j : GeometryInfo<dim>::face_indices())
    function_map[j] = &constant_function;

  for (const unsigned int i : GeometryInfo<dim>::face_indices())
    {
      deallog << "FE=" << fe.get_name() << ", case=" << i << std::endl;

      std::set<types::boundary_id> boundary_ids;
      for (unsigned int j = 0; j <= i; ++j)
        boundary_ids.insert(j);

      AffineConstraints<double> cm;
      VectorTools::compute_nonzero_normal_flux_constraints(
        dof, 0, boundary_ids, function_map, cm);

      cm.print(deallog.get_file_stream());
    }
  // Get the location of all boundary dofs
  std::vector<types::global_dof_index> face_dofs;
  const std::vector<Point<dim - 1>>   &unit_support_points =
    fe.get_unit_face_support_points();
  Quadrature<dim - 1>    quadrature(unit_support_points);
  FEFaceValues<dim, dim> fe_face_values(fe,
                                        quadrature,
                                        update_quadrature_points);
  typename DoFHandler<dim>::active_cell_iterator cell = dof.begin_active(),
                                                 endc = dof.end();
  for (; cell != endc; ++cell)
    for (const unsigned int face_no : GeometryInfo<dim>::face_indices())
      if (cell->face(face_no)->at_boundary())
        {
          typename DoFHandler<dim>::face_iterator face = cell->face(face_no);
          face_dofs.resize(fe.dofs_per_face);
          face->get_dof_indices(face_dofs);

          fe_face_values.reinit(cell, face_no);
          for (unsigned int i = 0; i < face_dofs.size(); ++i)
            {
              std::cout << face_dofs[i] << "\t"
                        << fe_face_values.quadrature_point(i) << "\t"
                        << fe.face_system_to_component_index(i).first
                        << std::endl;
            }
        }
}


template <int dim>
void
test_hyper_cube()
{
  Triangulation<dim> tr;
  GridGenerator::hyper_cube(tr);

  for (const unsigned int i : GeometryInfo<dim>::face_indices())
    tr.begin_active()->face(i)->set_boundary_id(i);

  tr.refine_global(2);

  for (unsigned int degree = 1; degree < 4; ++degree)
    {
      FESystem<dim> fe(FE_Q<dim>(degree), dim);
      test(tr, fe);
    }
}


int
main()
{
  initlog();
  deallog << std::setprecision(2);
  deallog << std::fixed;

  test_hyper_cube<2>();
  test_hyper_cube<3>();
}
