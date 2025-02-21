// ---------------------------------------------------------------------
//
// Copyright (C) 2018 - 2022 by the deal.II authors
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



// this function tests the correctness of the thread parallelization of the
// matrix-free class extensively

#include <deal.II/base/function.h>

#include <deal.II/fe/fe_dgq.h>

#include "../tests.h"

#include "create_mesh.h"
#include "matrix_vector_faces_common.h"


template <int dim, int fe_degree, typename number>
void
sub_test()
{
  Triangulation<dim> tria;
  create_mesh(tria);
  tria.begin_active()->set_refine_flag();
  tria.execute_coarsening_and_refinement();
  typename Triangulation<dim>::active_cell_iterator cell = tria.begin_active(),
                                                    endc = tria.end();
  for (; cell != endc; ++cell)
    if (cell->center().norm() < 0.5)
      cell->set_refine_flag();
  tria.execute_coarsening_and_refinement();
  tria.refine_global(1);
#ifndef DEBUG
  if (dim < 3 || fe_degree < 2)
    tria.refine_global(1);
  tria.begin(tria.n_levels() - 1)->set_refine_flag();
  tria.last()->set_refine_flag();
  tria.execute_coarsening_and_refinement();
  if (dim == 2 && fe_degree < 2)
    tria.refine_global(2);
  else
    tria.refine_global(1);
#endif

  FE_DGQ<dim>     fe(fe_degree);
  DoFHandler<dim> dof(tria);
  deallog << "Testing " << fe.get_name() << std::endl;

  // run test for several different meshes
  for (unsigned int i = 0; i < 8 - 2 * dim; ++i)
    {
      cell                 = tria.begin_active();
      endc                 = tria.end();
      unsigned int counter = 0;
      for (; cell != endc; ++cell, ++counter)
        if (counter % (9 - i) == 0)
          cell->set_refine_flag();
      tria.execute_coarsening_and_refinement();

      dof.distribute_dofs(fe);
      AffineConstraints<double> constraints;
      constraints.close();

      // std::cout << "Number of cells: " <<
      // dof.get_triangulation().n_active_cells() << std::endl; std::cout <<
      // "Number of degrees of freedom: " << dof.n_dofs() << std::endl;
      // std::cout
      // << "Number of constraints: " << constraints.n_constraints() <<
      // std::endl;

      MatrixFree<dim, number> mf_data, mf_data_color, mf_data_partition;
      {
        const QGauss<1> quad(fe_degree + 1);
        mf_data.reinit(MappingQ1<dim>{},
                       dof,
                       constraints,
                       quad,
                       typename MatrixFree<dim, number>::AdditionalData(
                         MatrixFree<dim, number>::AdditionalData::none));

        // choose block size of 3 which introduces
        // some irregularity to the blocks (stress the
        // non-overlapping computation harder)
        mf_data_color.reinit(
          MappingQ1<dim>{},
          dof,
          constraints,
          quad,
          typename MatrixFree<dim, number>::AdditionalData(
            MatrixFree<dim, number>::AdditionalData::partition_color, 3));

        mf_data_partition.reinit(
          MappingQ1<dim>{},
          dof,
          constraints,
          quad,
          typename MatrixFree<dim, number>::AdditionalData(
            MatrixFree<dim, number>::AdditionalData::partition_partition, 3));
      }

      MatrixFreeTest<dim, fe_degree, fe_degree + 1, number> mf_ref(mf_data);
      MatrixFreeTest<dim, fe_degree, fe_degree + 1, number> mf_color(
        mf_data_color);
      MatrixFreeTest<dim, fe_degree, fe_degree + 1, number> mf_partition(
        mf_data_partition);
      Vector<number> in_dist(dof.n_dofs());
      Vector<number> out_dist(in_dist), out_color(in_dist),
        out_partition(in_dist);

      for (unsigned int i = 0; i < dof.n_dofs(); ++i)
        {
          if (constraints.is_constrained(i))
            continue;
          const double entry = Testing::rand() / (double)RAND_MAX;
          in_dist(i)         = entry;
        }

      mf_ref.vmult(out_dist, in_dist);

      // make 10 sweeps in order to get in some
      // variation to the threaded program
      for (unsigned int sweep = 0; sweep < 10; ++sweep)
        {
          mf_color.vmult(out_color, in_dist);
          mf_partition.vmult(out_partition, in_dist);

          out_color -= out_dist;
          double diff_norm = out_color.linfty_norm();
          if (std::is_same_v<number, float> && diff_norm < 5e-6)
            diff_norm = 0;
          deallog << "Sweep " << sweep
                  << ", error in partition/color:                  "
                  << diff_norm << std::endl;
          out_partition -= out_dist;
          diff_norm = out_partition.linfty_norm();
          if (std::is_same_v<number, float> && diff_norm < 5e-6)
            diff_norm = 0;
          deallog << "Sweep " << sweep
                  << ", error in partition/partition:              "
                  << diff_norm << std::endl;
        }
      deallog << std::endl;
    }
  deallog << std::endl;
}


template <int dim, int fe_degree>
void
test()
{
  deallog << "Test doubles" << std::endl;
  sub_test<dim, fe_degree, double>();
  deallog << "Test floats" << std::endl;
  sub_test<dim, fe_degree, float>();
}
