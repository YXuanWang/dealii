// ---------------------------------------------------------------------
//
// Copyright (C) 2009 - 2020 by the deal.II authors
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



// same as assemble_matrix_parallel_02, but using a constraint that should
// create conflicts on most cells (similar to mean value constraint)

#include <deal.II/base/function.h>
#include <deal.II/base/graph_coloring.h>
#include <deal.II/base/quadrature_lib.h>
#include <deal.II/base/utilities.h>
#include <deal.II/base/work_stream.h>

#include <deal.II/dofs/dof_accessor.h>
#include <deal.II/dofs/dof_handler.h>
#include <deal.II/dofs/dof_tools.h>

#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_refinement.h>
#include <deal.II/grid/tria.h>
#include <deal.II/grid/tria_accessor.h>
#include <deal.II/grid/tria_iterator.h>

#include <deal.II/hp/fe_values.h>

#include <deal.II/lac/affine_constraints.h>
#include <deal.II/lac/dynamic_sparsity_pattern.h>
#include <deal.II/lac/full_matrix.h>
#include <deal.II/lac/precondition.h>
#include <deal.II/lac/solver_cg.h>
#include <deal.II/lac/sparse_matrix.h>
#include <deal.II/lac/vector.h>

#include <deal.II/numerics/error_estimator.h>
#include <deal.II/numerics/matrix_tools.h>
#include <deal.II/numerics/vector_tools.h>

#include <complex>
#include <iostream>

#include "../tests.h"



namespace Assembly
{
  namespace Scratch
  {
    template <int dim>
    struct Data
    {
      Data(const hp::FECollection<dim> &fe,
           const hp::QCollection<dim>  &quadrature)
        : hp_fe_values(fe,
                       quadrature,
                       update_values | update_gradients |
                         update_quadrature_points | update_JxW_values)
      {}

      Data(const Data &data)
        : hp_fe_values(data.hp_fe_values.get_mapping_collection(),
                       data.hp_fe_values.get_fe_collection(),
                       data.hp_fe_values.get_quadrature_collection(),
                       data.hp_fe_values.get_update_flags())
      {}

      hp::FEValues<dim> hp_fe_values;
    };
  } // namespace Scratch

  namespace Copy
  {
    struct Data
    {
      Data(const unsigned int test_variant)
        : test_variant(test_variant)
      {}

      Data(const Data &data)
        : test_variant(data.test_variant)
      {}

      std::vector<types::global_dof_index> local_dof_indices;
      FullMatrix<double>                   local_matrix;
      Vector<double>                       local_rhs;
      const unsigned int                   test_variant;
    };
  } // namespace Copy
} // namespace Assembly

template <int dim>
class LaplaceProblem
{
public:
  LaplaceProblem();
  ~LaplaceProblem();

  void
  run();

private:
  void
  setup_system();
  void
  test_equality();
  void
  assemble_reference();
  void
  assemble_test();
  void
  assemble_test_1();
  void
  assemble_test_2();
  void
  solve();
  void
  create_coarse_grid();
  void
  postprocess();

  void
  local_assemble(const typename DoFHandler<dim>::active_cell_iterator &cell,
                 Assembly::Scratch::Data<dim>                         &scratch,
                 Assembly::Copy::Data                                 &data);
  void
  copy_local_to_global(const Assembly::Copy::Data &data);

  std::vector<types::global_dof_index>
  get_conflict_indices(
    const typename DoFHandler<dim>::active_cell_iterator &cell) const;

  Triangulation<dim> triangulation;

  DoFHandler<dim>          dof_handler;
  hp::FECollection<dim>    fe_collection;
  hp::QCollection<dim>     quadrature_collection;
  hp::QCollection<dim - 1> face_quadrature_collection;

  AffineConstraints<double> constraints;

  SparsityPattern      sparsity_pattern;
  SparseMatrix<double> reference_matrix;
  SparseMatrix<double> test_matrix;
  SparseMatrix<double> test_matrix_2;

  Vector<double> solution;
  Vector<double> reference_rhs;
  Vector<double> test_rhs;
  Vector<double> test_rhs_2;

  std::vector<std::vector<typename DoFHandler<dim>::active_cell_iterator>>
    graph;

  const unsigned int max_degree;
};



template <int dim>
class BoundaryValues : public Function<dim>
{
public:
  BoundaryValues()
    : Function<dim>()
  {}

  virtual double
  value(const Point<dim> &p, const unsigned int component) const;
};


template <int dim>
double
BoundaryValues<dim>::value(const Point<dim> &p,
                           const unsigned int /*component*/) const
{
  double sum = 0;
  for (unsigned int d = 0; d < dim; ++d)
    sum += std::sin(numbers::PI * p[d]);
  return sum;
}


template <int dim>
class RightHandSide : public Function<dim>
{
public:
  RightHandSide()
    : Function<dim>()
  {}

  virtual double
  value(const Point<dim> &p, const unsigned int component) const;
};


template <int dim>
double
RightHandSide<dim>::value(const Point<dim> &p,
                          const unsigned int /*component*/) const
{
  double product = 1;
  for (unsigned int d = 0; d < dim; ++d)
    product *= (p[d] + 1);
  return product;
}


template <int dim>
LaplaceProblem<dim>::LaplaceProblem()
  : dof_handler(triangulation)
  , max_degree(5)
{
  if (dim == 2)
    for (unsigned int degree = 2; degree <= max_degree; ++degree)
      {
        fe_collection.push_back(FE_Q<dim>(degree));
        quadrature_collection.push_back(QGauss<dim>(degree + 1));
        face_quadrature_collection.push_back(QGauss<dim - 1>(degree + 1));
      }
  else
    for (unsigned int degree = 1; degree < max_degree - 1; ++degree)
      {
        fe_collection.push_back(FE_Q<dim>(degree));
        quadrature_collection.push_back(QGauss<dim>(degree + 1));
        face_quadrature_collection.push_back(QGauss<dim - 1>(degree + 1));
      }
}


template <int dim>
LaplaceProblem<dim>::~LaplaceProblem()
{
  dof_handler.clear();
}



template <int dim>
std::vector<types::global_dof_index>
LaplaceProblem<dim>::get_conflict_indices(
  const typename DoFHandler<dim>::active_cell_iterator &cell) const
{
  std::vector<types::global_dof_index> local_dof_indices(
    cell->get_fe().dofs_per_cell);
  cell->get_dof_indices(local_dof_indices);

  constraints.resolve_indices(local_dof_indices);
  return local_dof_indices;
}

template <int dim>
void
LaplaceProblem<dim>::setup_system()
{
  reference_matrix.clear();
  test_matrix.clear();
  dof_handler.distribute_dofs(fe_collection);

  solution.reinit(dof_handler.n_dofs());
  reference_rhs.reinit(dof_handler.n_dofs());
  test_rhs.reinit(dof_handler.n_dofs());
  test_rhs_2.reinit(dof_handler.n_dofs());

  constraints.clear();

  DoFTools::make_hanging_node_constraints(dof_handler, constraints);

  // add a constraint that expands over most of the domain, which should
  // create many conflicts. Avoid cycles and do not constrain to entries
  // already constrained
  for (unsigned int c = 0; c < dof_handler.n_dofs(); ++c)
    if (constraints.is_constrained(c) == false)
      {
        constraints.add_line(c);
        for (unsigned int i = 5; i < dof_handler.n_dofs(); i += 11)
          if (constraints.is_constrained(i) == false)
            constraints.add_entry(c, i, -0.5);
        break;
      }

  // add boundary conditions as inhomogeneous constraints here, do it after
  // having added the hanging node constraints in order to be consistent and
  // skip dofs that are already constrained (i.e., are hanging nodes on the
  // boundary in 3D). In contrast to step-27, we choose a sine function.
  VectorTools::interpolate_boundary_values(dof_handler,
                                           0,
                                           BoundaryValues<dim>(),
                                           constraints);
  constraints.close();

  graph = GraphColoring::make_graph_coloring(
    dof_handler.begin_active(),
    dof_handler.end(),
    static_cast<std::function<std::vector<types::global_dof_index>(
      const typename DoFHandler<dim>::active_cell_iterator &)>>(
      std::bind(&LaplaceProblem<dim>::get_conflict_indices,
                this,
                std::placeholders::_1)));


  DynamicSparsityPattern csp(dof_handler.n_dofs(), dof_handler.n_dofs());
  DoFTools::make_sparsity_pattern(dof_handler, csp, constraints, true);
  sparsity_pattern.copy_from(csp);

  reference_matrix.reinit(sparsity_pattern);
  test_matrix.reinit(sparsity_pattern);
  test_matrix_2.reinit(sparsity_pattern);
}



template <int dim>
void
LaplaceProblem<dim>::local_assemble(
  const typename DoFHandler<dim>::active_cell_iterator &cell,
  Assembly::Scratch::Data<dim>                         &scratch,
  Assembly::Copy::Data                                 &data)
{
  const unsigned int dofs_per_cell = cell->get_fe().dofs_per_cell;

  data.local_matrix.reinit(dofs_per_cell, dofs_per_cell);
  data.local_matrix = 0;

  data.local_rhs.reinit(dofs_per_cell);
  data.local_rhs = 0;

  scratch.hp_fe_values.reinit(cell);

  const FEValues<dim> &fe_values = scratch.hp_fe_values.get_present_fe_values();

  const RightHandSide<dim> rhs_function;

  for (unsigned int q_point = 0; q_point < fe_values.n_quadrature_points;
       ++q_point)
    {
      const double scale_mat = data.test_variant == 2 ? numbers::PI : 1.;
      const double rhs_value =
        rhs_function.value(fe_values.quadrature_point(q_point), 0);
      for (unsigned int i = 0; i < dofs_per_cell; ++i)
        {
          for (unsigned int j = 0; j < dofs_per_cell; ++j)
            data.local_matrix(i, j) +=
              (scale_mat * fe_values.shape_grad(i, q_point) *
               fe_values.shape_grad(j, q_point) * fe_values.JxW(q_point));

          data.local_rhs(i) += (fe_values.shape_value(i, q_point) * rhs_value *
                                scale_mat * fe_values.JxW(q_point));
        }
    }

  data.local_dof_indices.resize(dofs_per_cell);
  cell->get_dof_indices(data.local_dof_indices);
}



template <int dim>
void
LaplaceProblem<dim>::copy_local_to_global(const Assembly::Copy::Data &data)
{
  if (data.test_variant == 2)
    constraints.distribute_local_to_global(data.local_matrix,
                                           data.local_rhs,
                                           data.local_dof_indices,
                                           test_matrix_2,
                                           test_rhs_2);
  else if (data.test_variant == 1)
    constraints.distribute_local_to_global(data.local_matrix,
                                           data.local_rhs,
                                           data.local_dof_indices,
                                           test_matrix,
                                           test_rhs);
  else
    {
      for (unsigned int i = 0; i < data.local_dof_indices.size(); ++i)
        for (unsigned int j = 0; j < data.local_dof_indices.size(); ++j)
          reference_matrix.add(data.local_dof_indices[i],
                               data.local_dof_indices[j],
                               data.local_matrix(i, j));
      for (unsigned int i = 0; i < data.local_dof_indices.size(); ++i)
        reference_rhs(data.local_dof_indices[i]) += data.local_rhs(i);
    }
}



template <int dim>
void
LaplaceProblem<dim>::assemble_reference()
{
  reference_matrix = 0;
  reference_rhs    = 0;

  Assembly::Copy::Data         copy_data(0);
  Assembly::Scratch::Data<dim> assembly_data(fe_collection,
                                             quadrature_collection);

  for (unsigned int color = 0; color < graph.size(); ++color)
    for (typename std::vector<
           typename DoFHandler<dim>::active_cell_iterator>::const_iterator p =
           graph[color].begin();
         p != graph[color].end();
         ++p)
      {
        local_assemble(*p, assembly_data, copy_data);
        copy_local_to_global(copy_data);
      }
  constraints.condense(reference_matrix, reference_rhs);

  // since constrained diagonal entries might be different, zero them out here
  for (unsigned int i = 0; i < dof_handler.n_dofs(); ++i)
    if (constraints.is_constrained(i))
      {
        reference_matrix.diag_element(i) = 0;
        reference_rhs(i)                 = 0;
      }
}



template <int dim>
void
LaplaceProblem<dim>::assemble_test_1()
{
  test_matrix = 0;
  test_rhs    = 0;

  WorkStream::run(graph,
                  std::bind(&LaplaceProblem<dim>::local_assemble,
                            this,
                            std::placeholders::_1,
                            std::placeholders::_2,
                            std::placeholders::_3),
                  std::bind(&LaplaceProblem<dim>::copy_local_to_global,
                            this,
                            std::placeholders::_1),
                  Assembly::Scratch::Data<dim>(fe_collection,
                                               quadrature_collection),
                  Assembly::Copy::Data(1),
                  2 * MultithreadInfo::n_threads(),
                  1);
  for (unsigned int i = 0; i < dof_handler.n_dofs(); ++i)
    if (constraints.is_constrained(i))
      {
        test_matrix.diag_element(i) = 0;
        test_rhs(i)                 = 0;
      }
}


template <int dim>
void
LaplaceProblem<dim>::assemble_test_2()
{
  test_matrix_2 = 0;
  test_rhs_2    = 0;

  WorkStream::run(graph,
                  std::bind(&LaplaceProblem<dim>::local_assemble,
                            this,
                            std::placeholders::_1,
                            std::placeholders::_2,
                            std::placeholders::_3),
                  std::bind(&LaplaceProblem<dim>::copy_local_to_global,
                            this,
                            std::placeholders::_1),
                  Assembly::Scratch::Data<dim>(fe_collection,
                                               quadrature_collection),
                  Assembly::Copy::Data(2),
                  2 * MultithreadInfo::n_threads(),
                  1);
  for (unsigned int i = 0; i < dof_handler.n_dofs(); ++i)
    if (constraints.is_constrained(i))
      {
        test_matrix_2.diag_element(i) = 0;
        test_rhs_2(i)                 = 0;
      }
}

template <int dim>
void
LaplaceProblem<dim>::assemble_test()
{
  // start two tasks that each run an assembly
  Threads::TaskGroup<void> tasks;
  tasks += Threads::new_task(&LaplaceProblem<dim>::assemble_test_1, *this);
  tasks += Threads::new_task(&LaplaceProblem<dim>::assemble_test_2, *this);
  tasks.join_all();

  test_matrix.add(-1, reference_matrix);
  deallog << "error in matrix 1: " << test_matrix.frobenius_norm() << std::endl;
  test_rhs.add(-1., reference_rhs);
  deallog << "error in vector 1: " << test_rhs.l2_norm() << std::endl;

  test_matrix_2.add(-numbers::PI, reference_matrix);
  deallog << "error in matrix 2: " << test_matrix_2.frobenius_norm()
          << std::endl;
  test_rhs_2.add(-numbers::PI, reference_rhs);
  deallog << "error in vector 2: " << test_rhs_2.l2_norm() << std::endl;
}



template <int dim>
void
LaplaceProblem<dim>::postprocess()
{
  Vector<float> estimated_error_per_cell(triangulation.n_active_cells());
  for (unsigned int i = 0; i < estimated_error_per_cell.size(); ++i)
    estimated_error_per_cell(i) = i;

  GridRefinement::refine_and_coarsen_fixed_number(triangulation,
                                                  estimated_error_per_cell,
                                                  0.3,
                                                  0.03);
  triangulation.execute_coarsening_and_refinement();

  for (typename DoFHandler<dim>::active_cell_iterator cell =
         dof_handler.begin_active();
       cell != dof_handler.end();
       ++cell)
    cell->set_active_fe_index(rand() % fe_collection.size());
}



template <int dim>
void
LaplaceProblem<dim>::run()
{
  for (unsigned int cycle = 0; cycle < 3; ++cycle)
    {
      if (cycle == 0)
        {
          GridGenerator::hyper_shell(
            triangulation, Point<dim>(), 0.5, 1., (dim == 3) ? 96 : 12, false);
          triangulation.refine_global(2);
        }

      setup_system();

      assemble_reference();
      assemble_test();

      if (cycle < 2)
        postprocess();
    }
}



int
main()
{
  initlog();
  deallog << std::setprecision(2);

  {
    deallog.push("2d");
    LaplaceProblem<2> laplace_problem;
    laplace_problem.run();
    deallog.pop();
  }
}
