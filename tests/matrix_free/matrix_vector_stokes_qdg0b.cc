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



// this function tests the correctness of the implementation of matrix free
// matrix-vector products by comparing with the result of deal.II sparse
// matrix. No hanging nodes and no other constraints for a vector-valued
// problem (stokes equations). Same as matrix_vector_stokes_qdg0 but now
// without the template on the degree of the element.

#include <deal.II/base/utilities.h>

#include <deal.II/dofs/dof_handler.h>
#include <deal.II/dofs/dof_renumbering.h>
#include <deal.II/dofs/dof_tools.h>

#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_q_dg0.h>
#include <deal.II/fe/fe_system.h>
#include <deal.II/fe/fe_values.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/manifold_lib.h>
#include <deal.II/grid/tria.h>

#include <deal.II/lac/affine_constraints.h>
#include <deal.II/lac/block_sparse_matrix.h>
#include <deal.II/lac/block_sparsity_pattern.h>
#include <deal.II/lac/block_vector.h>

#include <deal.II/matrix_free/fe_evaluation.h>
#include <deal.II/matrix_free/matrix_free.h>

#include <deal.II/numerics/vector_tools.h>

#include <complex>
#include <iostream>

#include "../tests.h"

#include "create_mesh.h"



template <int dim, typename VectorType>
class MatrixFreeTest
{
public:
  using CellIterator = typename DoFHandler<dim>::active_cell_iterator;
  using Number       = double;

  MatrixFreeTest(const MatrixFree<dim, Number> &data_in)
    : data(data_in){};

  void
  local_apply(const MatrixFree<dim, Number>               &data,
              VectorType                                  &dst,
              const VectorType                            &src,
              const std::pair<unsigned int, unsigned int> &cell_range) const
  {
    using vector_t = VectorizedArray<Number>;
    FEEvaluation<dim, -1, 0, dim, Number> velocity(data, 0);
    FEEvaluation<dim, -1, 0, 1, Number>   pressure(data, 1);

    for (unsigned int cell = cell_range.first; cell < cell_range.second; ++cell)
      {
        velocity.reinit(cell);
        velocity.read_dof_values(src, 0);
        velocity.evaluate(EvaluationFlags::gradients);
        pressure.reinit(cell);
        pressure.read_dof_values(src, dim);
        pressure.evaluate(EvaluationFlags::values);

        for (unsigned int q = 0; q < velocity.n_q_points; ++q)
          {
            SymmetricTensor<2, dim, vector_t> sym_grad_u =
              velocity.get_symmetric_gradient(q);
            vector_t pres = pressure.get_value(q);
            vector_t div  = -velocity.get_divergence(q);
            pressure.submit_value(div, q);

            // subtract p * I
            for (unsigned int d = 0; d < dim; ++d)
              sym_grad_u[d][d] -= pres;

            velocity.submit_symmetric_gradient(sym_grad_u, q);
          }

        velocity.integrate(EvaluationFlags::gradients);
        velocity.distribute_local_to_global(dst, 0);
        pressure.integrate(EvaluationFlags::values);
        pressure.distribute_local_to_global(dst, dim);
      }
  }


  void
  vmult(VectorType &dst, const VectorType &src) const
  {
    AssertDimension(dst.size(), dim + 1);
    for (unsigned int d = 0; d < dim + 1; ++d)
      dst[d] = 0;
    data.cell_loop(&MatrixFreeTest::local_apply, this, dst, src);
  };

private:
  const MatrixFree<dim, Number> &data;
};



template <int dim>
void
test(const unsigned int fe_degree)
{
  Triangulation<dim> triangulation;
  create_mesh(triangulation);
  if (fe_degree == 1)
    triangulation.refine_global(4 - dim);
  else
    triangulation.refine_global(3 - dim);

  FE_Q<dim>       fe_u(fe_degree + 1);
  FE_Q_DG0<dim>   fe_p(fe_degree);
  FESystem<dim>   fe(fe_u, dim, fe_p, 1);
  DoFHandler<dim> dof_handler_u(triangulation);
  DoFHandler<dim> dof_handler_p(triangulation);
  DoFHandler<dim> dof_handler(triangulation);

  MatrixFree<dim, double> mf_data;

  AffineConstraints<double> constraints;

  BlockSparsityPattern      sparsity_pattern;
  BlockSparseMatrix<double> system_matrix;

  BlockVector<double>         solution;
  BlockVector<double>         system_rhs;
  std::vector<Vector<double>> vec1, vec2;

  dof_handler.distribute_dofs(fe);
  dof_handler_u.distribute_dofs(fe_u);
  dof_handler_p.distribute_dofs(fe_p);
  DoFRenumbering::component_wise(dof_handler);

  constraints.close();

  const std::vector<types::global_dof_index> dofs_per_block =
    DoFTools::count_dofs_per_fe_component(dof_handler);

  // std::cout << "   Number of active cells: "
  //          << triangulation.n_active_cells()
  //          << std::endl
  //          << "   Number of degrees of freedom: "
  //          << dof_handler.n_dofs()
  //          << " (" << n_u << '+' << n_p << ')'
  //          << std::endl;

  {
    BlockDynamicSparsityPattern csp(dim + 1, dim + 1);

    for (unsigned int d = 0; d < dim + 1; ++d)
      for (unsigned int e = 0; e < dim + 1; ++e)
        csp.block(d, e).reinit(dofs_per_block[d], dofs_per_block[e]);

    csp.collect_sizes();

    DoFTools::make_sparsity_pattern(dof_handler, csp, constraints, false);
    sparsity_pattern.copy_from(csp);
  }

  system_matrix.reinit(sparsity_pattern);

  solution.reinit(dim + 1);
  for (unsigned int i = 0; i < dim + 1; ++i)
    solution.block(i).reinit(dofs_per_block[i]);
  solution.collect_sizes();

  system_rhs.reinit(solution);

  vec1.resize(dim + 1);
  vec2.resize(dim + 1);
  vec1[0].reinit(dofs_per_block[0]);
  vec2[0].reinit(vec1[0]);
  for (unsigned int i = 1; i < dim; ++i)
    {
      vec1[i].reinit(vec1[0]);
      vec2[i].reinit(vec1[0]);
    }
  vec1[dim].reinit(dofs_per_block[dim]);
  vec2[dim].reinit(vec1[dim]);

  // this is from step-22
  {
    QGauss<dim> quadrature_formula(fe_degree + 2);

    FEValues<dim> fe_values(fe,
                            quadrature_formula,
                            update_values | update_JxW_values |
                              update_gradients);

    const unsigned int dofs_per_cell = fe.dofs_per_cell;
    const unsigned int n_q_points    = quadrature_formula.size();

    FullMatrix<double> local_matrix(dofs_per_cell, dofs_per_cell);

    std::vector<types::global_dof_index> local_dof_indices(dofs_per_cell);

    const FEValuesExtractors::Vector velocities(0);
    const FEValuesExtractors::Scalar pressure(dim);

    std::vector<SymmetricTensor<2, dim>> phi_grads_u(dofs_per_cell);
    std::vector<double>                  div_phi_u(dofs_per_cell);
    std::vector<double>                  phi_p(dofs_per_cell);

    typename DoFHandler<dim>::active_cell_iterator cell =
                                                     dof_handler.begin_active(),
                                                   endc = dof_handler.end();
    for (; cell != endc; ++cell)
      {
        fe_values.reinit(cell);
        local_matrix = 0;

        for (unsigned int q = 0; q < n_q_points; ++q)
          {
            for (unsigned int k = 0; k < dofs_per_cell; ++k)
              {
                phi_grads_u[k] = fe_values[velocities].symmetric_gradient(k, q);
                div_phi_u[k]   = fe_values[velocities].divergence(k, q);
                phi_p[k]       = fe_values[pressure].value(k, q);
              }

            for (unsigned int i = 0; i < dofs_per_cell; ++i)
              {
                for (unsigned int j = 0; j <= i; ++j)
                  {
                    local_matrix(i, j) +=
                      (phi_grads_u[i] * phi_grads_u[j] -
                       div_phi_u[i] * phi_p[j] - phi_p[i] * div_phi_u[j]) *
                      fe_values.JxW(q);
                  }
              }
          }
        for (unsigned int i = 0; i < dofs_per_cell; ++i)
          for (unsigned int j = i + 1; j < dofs_per_cell; ++j)
            local_matrix(i, j) = local_matrix(j, i);

        cell->get_dof_indices(local_dof_indices);
        constraints.distribute_local_to_global(local_matrix,
                                               local_dof_indices,
                                               system_matrix);
      }
  }

  // first system_rhs with random numbers
  for (unsigned int i = 0; i < dim + 1; ++i)
    for (unsigned int j = 0; j < system_rhs.block(i).size(); ++j)
      {
        const double val       = -1. + 2. * random_value<double>();
        system_rhs.block(i)(j) = val;
        vec1[i](j)             = val;
      }

  // setup matrix-free structure
  {
    std::vector<const DoFHandler<dim> *> dofs;
    dofs.push_back(&dof_handler_u);
    dofs.push_back(&dof_handler_p);
    AffineConstraints<double> dummy_constraints;
    dummy_constraints.close();
    std::vector<const AffineConstraints<double> *> constraints;
    constraints.push_back(&dummy_constraints);
    constraints.push_back(&dummy_constraints);
    QGauss<1> quad(fe_degree + 2);
    mf_data.reinit(MappingQ1<dim>{},
                   dofs,
                   constraints,
                   quad,
                   typename MatrixFree<dim>::AdditionalData(
                     MatrixFree<dim>::AdditionalData::none));
  }

  system_matrix.vmult(solution, system_rhs);

  using VectorType = std::vector<Vector<double>>;
  MatrixFreeTest<dim, VectorType> mf(mf_data);
  mf.vmult(vec2, vec1);

  // Verification
  double error = 0.;
  for (unsigned int i = 0; i < dim + 1; ++i)
    for (unsigned int j = 0; j < system_rhs.block(i).size(); ++j)
      error += std::fabs(solution.block(i)(j) - vec2[i](j));
  double relative = solution.block(0).l1_norm();
  deallog << "  Verification fe degree " << fe_degree << ": "
          << error / relative << std::endl
          << std::endl;
}



int
main()
{
  initlog();
  deallog << std::setprecision(3);

  {
    deallog << std::endl << "Test with doubles" << std::endl << std::endl;
    deallog.push("2d");
    test<2>(1);
    test<2>(2);
    test<2>(3);
    deallog.pop();
    deallog.push("3d");
    test<3>(1);
    test<3>(2);
    deallog.pop();
  }
}
