// ---------------------------------------------------------------------
//
// Copyright (C) 2023 by the deal.II authors
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



// this function tests the correctness of the implementation of matrix-free
// operations in getting the function values, the function gradients, and the
// function Laplacians on a structured mesh (hyper cube) with deformed
// elements inside of the domain. This tests whether non-affine geometries
// with more complicated terms for the Jacobian are treated correctly. The
// test case is without any constraints.

#include <deal.II/base/function.h>
#include <deal.II/base/logstream.h>
#include <deal.II/base/utilities.h>

#include <deal.II/dofs/dof_handler.h>
#include <deal.II/dofs/dof_tools.h>

#include <deal.II/fe/fe_raviart_thomas.h>
#include <deal.II/fe/mapping_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/manifold_lib.h>
#include <deal.II/grid/tria.h>

#include <deal.II/lac/affine_constraints.h>
#include <deal.II/lac/vector.h>

#include <deal.II/matrix_free/fe_evaluation.h>
#include <deal.II/matrix_free/matrix_free.h>

#include <deal.II/numerics/vector_tools.h>

#include <fstream>
#include <iostream>

#include "../tests.h"


template <int dim>
class CompareFunction : public Function<dim>
{
public:
  CompareFunction()
    : Function<dim>(dim)
  {}

  virtual double
  value(const Point<dim> &p, const unsigned int component) const
  {
    double value = (1.2 - 0.5 * component) * p[0] * p[0] + 0.4 + component;
    for (unsigned int d = 1; d < dim; ++d)
      value -= (2.7 - 0.6 * component) * d * p[d] * p[d];
    return value;
  }

  virtual Tensor<1, dim>
  gradient(const Point<dim> &p, const unsigned int component) const
  {
    Tensor<1, dim> grad;
    grad[0] = (1.2 - 0.5 * component) * p[0] * 2;
    for (unsigned int d = 1; d < dim; ++d)
      grad[d] = -(2.7 - 0.6 * component) * d * p[d] * 2;
    return grad;
  }
};



template <int dim,
          int fe_degree,
          int n_q_points_1d = fe_degree + 1,
          typename Number   = double>
class MatrixFreeTest
{
public:
  MatrixFreeTest(const MatrixFree<dim, Number> &data_in)
    : data(data_in){};

  MatrixFreeTest(const MatrixFree<dim, Number> &data_in,
                 const Mapping<dim>            &mapping)
    : data(data_in){};

  virtual ~MatrixFreeTest()
  {}

  // make function virtual to allow derived classes to define a different
  // function
  virtual void
  cell(const MatrixFree<dim, Number> &data,
       Vector<Number> &,
       const Vector<Number>                        &src,
       const std::pair<unsigned int, unsigned int> &cell_range) const
  {
    FEEvaluation<dim, fe_degree, n_q_points_1d, dim, Number> fe_eval(data);

    CompareFunction<dim> function;

    for (unsigned int cell = cell_range.first; cell < cell_range.second; ++cell)
      {
        fe_eval.reinit(cell);
        fe_eval.read_dof_values(src);
        fe_eval.evaluate(EvaluationFlags::values | EvaluationFlags::gradients);

        for (unsigned int j = 0; j < data.n_active_entries_per_cell_batch(cell);
             ++j)
          for (unsigned int q = 0; q < fe_eval.n_q_points; ++q)
            {
              Point<dim> p;
              for (unsigned int d = 0; d < dim; ++d)
                p[d] = fe_eval.quadrature_point(q)[d][j];
              for (unsigned int d = 0; d < dim; ++d)
                {
                  cell_errors[0][d] += std::abs(fe_eval.get_value(q)[d][j] -
                                                function.value(p, d)) *
                                       fe_eval.JxW(q)[j];
                  for (unsigned int e = 0; e < dim; ++e)
                    cell_errors[1][d] +=
                      std::abs(fe_eval.get_gradient(q)[d][e][j] -
                               function.gradient(p, d)[e]) *
                      fe_eval.JxW(q)[j];
                }
              double divergence = 0;
              for (unsigned int d = 0; d < dim; ++d)
                divergence += function.gradient(p, d)[d];
              cell_errors[2][0] +=
                std::abs(fe_eval.get_divergence(q)[j] - divergence) *
                fe_eval.JxW(q)[j];
            }
      }
  }

  virtual void
  face(const MatrixFree<dim, Number> &data,
       Vector<Number> &,
       const Vector<Number>                        &src,
       const std::pair<unsigned int, unsigned int> &face_range) const
  {
    FEFaceEvaluation<dim, fe_degree, n_q_points_1d, dim, Number> fe_evalm(data,
                                                                          true);
    FEFaceEvaluation<dim, fe_degree, n_q_points_1d, dim, Number> fe_evalp(
      data, false);

    CompareFunction<dim> function;

    for (unsigned int face = face_range.first; face < face_range.second; ++face)
      {
        fe_evalm.reinit(face);
        fe_evalm.read_dof_values(src);
        fe_evalm.evaluate(EvaluationFlags::values | EvaluationFlags::gradients |
                          EvaluationFlags::hessians);
        fe_evalp.reinit(face);
        fe_evalp.read_dof_values(src);
        fe_evalp.evaluate(EvaluationFlags::values | EvaluationFlags::gradients |
                          EvaluationFlags::hessians);

        for (unsigned int j = 0; j < VectorizedArray<Number>::size(); ++j)
          {
            // skip empty components in VectorizedArray
            if (data.get_face_info(face).cells_interior[j] ==
                numbers::invalid_unsigned_int)
              break;
            for (unsigned int q = 0; q < fe_evalm.n_q_points; ++q)
              {
                Point<dim> p;

                // interior face
                for (unsigned int d = 0; d < dim; ++d)
                  p[d] = fe_evalm.quadrature_point(q)[d][j];
                for (unsigned int d = 0; d < dim; ++d)
                  {
                    facem_errors[0][d] += std::abs(fe_evalm.get_value(q)[d][j] -
                                                   function.value(p, d)) *
                                          fe_evalm.JxW(q)[j];
                    for (unsigned int e = 0; e < dim; ++e)
                      {
                        facem_errors[1][d] +=
                          std::abs(fe_evalm.get_gradient(q)[d][e][j] -
                                   function.gradient(p, d)[e]) *
                          fe_evalm.JxW(q)[j];
                      }
                  }
                double divergence = 0;
                for (unsigned int d = 0; d < dim; ++d)
                  divergence += function.gradient(p, d)[d];
                facem_errors[2][0] +=
                  std::abs(fe_evalm.get_divergence(q)[j] - divergence) *
                  fe_evalm.JxW(q)[j];

                // exterior face
                for (unsigned int d = 0; d < dim; ++d)
                  {
                    facep_errors[0][d] += std::abs(fe_evalp.get_value(q)[d][j] -
                                                   function.value(p, d)) *
                                          fe_evalm.JxW(q)[j];
                    for (unsigned int e = 0; e < dim; ++e)
                      facep_errors[1][d] +=
                        std::abs(fe_evalp.get_gradient(q)[d][e][j] -
                                 function.gradient(p, d)[e]) *
                        fe_evalm.JxW(q)[j];
                  }
                facep_errors[2][0] +=
                  std::abs(fe_evalp.get_divergence(q)[j] - divergence) *
                  fe_evalm.JxW(q)[j];
              }
          }
      }
  }

  virtual void
  boundary(const MatrixFree<dim, Number> &data,
           Vector<Number> &,
           const Vector<Number>                        &src,
           const std::pair<unsigned int, unsigned int> &face_range) const
  {
    FEFaceEvaluation<dim, fe_degree, n_q_points_1d, dim, Number> fe_evalm(data,
                                                                          true);

    CompareFunction<dim> function;

    for (unsigned int face = face_range.first; face < face_range.second; ++face)
      {
        fe_evalm.reinit(face);
        fe_evalm.read_dof_values(src);
        fe_evalm.evaluate(EvaluationFlags::values | EvaluationFlags::gradients |
                          EvaluationFlags::hessians);

        for (unsigned int j = 0; j < VectorizedArray<Number>::size(); ++j)
          {
            // skip empty components in VectorizedArray
            if (data.get_face_info(face).cells_interior[j] ==
                numbers::invalid_unsigned_int)
              break;
            for (unsigned int q = 0; q < fe_evalm.n_q_points; ++q)
              {
                Point<dim> p;
                for (unsigned int d = 0; d < dim; ++d)
                  p[d] = fe_evalm.quadrature_point(q)[d][j];
                for (unsigned int d = 0; d < dim; ++d)
                  {
                    boundary_errors[0][d] +=
                      std::abs(fe_evalm.get_value(q)[d][j] -
                               function.value(p, d)) *
                      fe_evalm.JxW(q)[j];
                    for (unsigned int e = 0; e < dim; ++e)
                      boundary_errors[1][d] +=
                        std::abs(fe_evalm.get_gradient(q)[d][e][j] -
                                 function.gradient(p, d)[e]) *
                        fe_evalm.JxW(q)[j];
                  }
                double divergence = 0;
                for (unsigned int d = 0; d < dim; ++d)
                  divergence += function.gradient(p, d)[d];
                boundary_errors[2][0] +=
                  std::abs(fe_evalm.get_divergence(q)[j] - divergence) *
                  fe_evalm.JxW(q)[j];
              }
          }
      }
  }



  static void
  print_error(const std::string                     &text,
              const dealii::ndarray<double, 3, dim> &array)
  {
    deallog << "Error " << std::left << std::setw(6) << text << " values:     ";
    for (unsigned int d = 0; d < dim; ++d)
      deallog << array[0][d] << " ";
    deallog << std::endl;
    deallog << "Error " << std::left << std::setw(6) << text << " gradients:  ";
    for (unsigned int d = 0; d < dim; ++d)
      deallog << array[1][d] << " ";
    deallog << std::endl;
    deallog << "Error " << std::left << std::setw(6) << text << " divergence: ";
    deallog << array[2][0] << " ";
    deallog << std::endl;
  }

  void
  test_functions(const Vector<Number> &src) const
  {
    for (unsigned int d = 0; d < dim; ++d)
      for (unsigned int i = 0; i < 3; ++i)
        {
          cell_errors[i][d]     = 0;
          facem_errors[i][d]    = 0;
          facep_errors[i][d]    = 0;
          boundary_errors[i][d] = 0;
        }

    Vector<Number> dst_dummy;
    data.loop(&MatrixFreeTest::cell,
              &MatrixFreeTest::face,
              &MatrixFreeTest::boundary,
              this,
              dst_dummy,
              src);

    print_error("cell", cell_errors);
    print_error("face-", facem_errors);
    print_error("face+", facep_errors);
    print_error("face b", boundary_errors);
    deallog << std::endl;
  };

protected:
  const MatrixFree<dim, Number>          &data;
  mutable dealii::ndarray<double, 3, dim> cell_errors, facem_errors,
    facep_errors, boundary_errors;
};



template <int dim, int fe_degree, typename number>
void
do_test(const DoFHandler<dim>           &dof,
        const AffineConstraints<double> &constraints)
{
  deallog << "Testing " << dof.get_fe().get_name() << std::endl;
  // use this for info on problem
  // std::cout << "Number of cells: " <<
  // dof.get_triangulation().n_active_cells()
  //          << std::endl;
  // std::cout << "Number of degrees of freedom: " << dof.n_dofs() << std::endl;
  // std::cout << "Number of constraints: " << constraints.n_constraints() <<
  // std::endl;

  MappingQ<dim>  mapping(dof.get_fe().degree);
  Vector<number> interpolated(dof.n_dofs());
  VectorTools::interpolate(mapping, dof, CompareFunction<dim>(), interpolated);

  constraints.distribute(interpolated);
  MatrixFree<dim, number> mf_data;
  {
    const QGauss<1> quad(dof.get_fe().degree + 1);
    typename MatrixFree<dim, number>::AdditionalData data;
    data.tasks_parallel_scheme = MatrixFree<dim, number>::AdditionalData::none;
    data.mapping_update_flags  = update_gradients | update_quadrature_points;
    data.mapping_update_flags_boundary_faces =
      update_gradients | update_quadrature_points;
    data.mapping_update_flags_inner_faces =
      update_gradients | update_quadrature_points;
    mf_data.reinit(mapping, dof, constraints, quad, data);
  }

  MatrixFreeTest<dim, fe_degree, fe_degree + 1, number> mf(mf_data);
  mf.test_functions(interpolated);
}



template <int dim>
class DeformedCubeManifold : public dealii::ChartManifold<dim, dim, dim>
{
public:
  DeformedCubeManifold(const double       left,
                       const double       right,
                       const double       deformation,
                       const unsigned int frequency = 1)
    : left(left)
    , right(right)
    , deformation(deformation)
    , frequency(frequency)
  {}

  dealii::Point<dim>
  push_forward(const dealii::Point<dim> &chart_point) const override
  {
    double sinval = deformation;
    for (unsigned int d = 0; d < dim; ++d)
      sinval *= std::sin(frequency * dealii::numbers::PI *
                         (chart_point(d) - left) / (right - left));
    dealii::Point<dim> space_point;
    for (unsigned int d = 0; d < dim; ++d)
      space_point(d) = chart_point(d) + sinval;
    return space_point;
  }

  dealii::Point<dim>
  pull_back(const dealii::Point<dim> &space_point) const override
  {
    dealii::Point<dim> x = space_point;
    dealii::Point<dim> one;
    for (unsigned int d = 0; d < dim; ++d)
      one(d) = 1.;

    // Newton iteration to solve the nonlinear equation given by the point
    dealii::Tensor<1, dim> sinvals;
    for (unsigned int d = 0; d < dim; ++d)
      sinvals[d] = std::sin(frequency * dealii::numbers::PI * (x(d) - left) /
                            (right - left));

    double sinval = deformation;
    for (unsigned int d = 0; d < dim; ++d)
      sinval *= sinvals[d];
    dealii::Tensor<1, dim> residual = space_point - x - sinval * one;
    unsigned int           its      = 0;
    while (residual.norm() > 1e-12 && its < 100)
      {
        dealii::Tensor<2, dim> jacobian;
        for (unsigned int d = 0; d < dim; ++d)
          jacobian[d][d] = 1.;
        for (unsigned int d = 0; d < dim; ++d)
          {
            double sinval_der = deformation * frequency / (right - left) *
                                dealii::numbers::PI *
                                std::cos(frequency * dealii::numbers::PI *
                                         (x(d) - left) / (right - left));
            for (unsigned int e = 0; e < dim; ++e)
              if (e != d)
                sinval_der *= sinvals[e];
            for (unsigned int e = 0; e < dim; ++e)
              jacobian[e][d] += sinval_der;
          }

        x += invert(jacobian) * residual;

        for (unsigned int d = 0; d < dim; ++d)
          sinvals[d] = std::sin(frequency * dealii::numbers::PI *
                                (x(d) - left) / (right - left));

        sinval = deformation;
        for (unsigned int d = 0; d < dim; ++d)
          sinval *= sinvals[d];
        residual = space_point - x - sinval * one;
        ++its;
      }
    AssertThrow(residual.norm() < 1e-12,
                dealii::ExcMessage("Newton for point did not converge."));
    return x;
  }

  std::unique_ptr<dealii::Manifold<dim>>
  clone() const override
  {
    return std::make_unique<DeformedCubeManifold<dim>>(left,
                                                       right,
                                                       deformation,
                                                       frequency);
  }

private:
  const double       left;
  const double       right;
  const double       deformation;
  const unsigned int frequency;
};



template <int dim, int fe_degree>
void
test()
{
  Triangulation<dim> tria;
  Point<dim>         left;
  Point<dim>         right;
  right[0] = 2.;
  right[1] = 1.;
  if (dim > 2)
    right[2] = 1.;
  GridGenerator::hyper_rectangle(tria, left, right);
  DeformedCubeManifold<dim> manifold(0, 1.0, 0.05, 1.);
  tria.set_all_manifold_ids(1);
  tria.set_manifold(1, manifold);
  tria.refine_global(1);

  {
    FE_RaviartThomasNodal<dim> fe(fe_degree - 1);
    DoFHandler<dim>            dof(tria);
    dof.distribute_dofs(fe);

    AffineConstraints<double> constraints;
    constraints.close();
    deallog.push("L1");
    if (fe_degree > 2)
      do_test<dim, -1, double>(dof, constraints);
    else
      do_test<dim, fe_degree, double>(dof, constraints);
    deallog.pop();

    tria.refine_global(1);
    dof.distribute_dofs(fe);
    deallog.push("L2");
    if (fe_degree > 2)
      do_test<dim, -1, double>(dof, constraints);
    else
      do_test<dim, fe_degree, double>(dof, constraints);
    deallog.pop();
  }
}



int
main()
{
  initlog();

  deallog << std::setprecision(5);
  {
    deallog.push("2d");
    test<2, 1>();
    test<2, 2>();
    test<2, 3>();
    test<2, 4>();
    deallog.pop();
    deallog.push("3d");
    test<3, 1>();
    test<3, 2>();
    test<3, 3>();
    deallog.pop();
  }
}
