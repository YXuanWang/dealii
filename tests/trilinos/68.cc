// ---------------------------------------------------------------------
//
// Copyright (C) 2004 - 2020 by the deal.II authors
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



// check TrilinosWrappers::MatrixBase::clear_row () with used second argument

#include <deal.II/base/utilities.h>

#include <deal.II/lac/trilinos_sparse_matrix.h>
#include <deal.II/lac/vector.h>

#include <iostream>
#include <vector>

#include "../tests.h"


void
test(TrilinosWrappers::SparseMatrix &m)
{
  Assert(m.m() != 0, ExcInternalError());
  Assert(m.n() != 0, ExcInternalError());

  // build a tri-diagonal pattern
  double             norm_sqr = 0;
  unsigned int       nnz      = 0;
  const unsigned int N        = m.m();
  for (unsigned int i = 0; i < N; ++i)
    {
      if (i >= 5)
        {
          const double s = Testing::rand();
          m.set(i, i - 5, s);
          norm_sqr += s * s;
          ++nnz;
        }

      if (i < N - 5)
        {
          const double s = Testing::rand();
          m.set(i, i + 5, s);
          norm_sqr += s * s;
          ++nnz;
        }

      const double s = Testing::rand();
      m.set(i, i, s);
      norm_sqr += s * s;
      ++nnz;
    }
  m.compress(VectorOperation::insert);

  deallog << m.frobenius_norm() << ' ' << std::sqrt(norm_sqr) << std::endl;
  deallog << m.n_nonzero_elements() << ' ' << nnz << std::endl;

  Assert(std::fabs(m.frobenius_norm() - std::sqrt(norm_sqr)) <
           std::fabs(std::sqrt(norm_sqr)),
         ExcInternalError());
  Assert(m.n_nonzero_elements() - nnz == 0, ExcInternalError());

  // now remove the entries of row N/2. set
  // diagonal entries to rnd
  const double rnd = Testing::rand();
  for (unsigned int i = 0; i < N; ++i)
    {
      const double s = m.el(N / 2, i);
      norm_sqr -= s * s;
    }
  norm_sqr += rnd * rnd;

  m.clear_row(N / 2, rnd);

  Assert(m.el(N / 2, N / 2) == rnd, ExcInternalError());

  deallog << m.frobenius_norm() << ' ' << std::sqrt(norm_sqr) << std::endl;
  deallog << m.n_nonzero_elements() << ' ' << nnz << std::endl;

  Assert(std::fabs(m.frobenius_norm() - std::sqrt(norm_sqr)) <
           std::fabs(std::sqrt(norm_sqr)),
         ExcInternalError());

  // make sure that zeroing out rows does at
  // least not add new nonzero entries (it
  // may remove some, though)
  Assert(m.n_nonzero_elements() <= nnz, ExcInternalError());

  deallog << "OK" << std::endl;
}



int
main(int argc, char **argv)
{
  initlog();

  Utilities::MPI::MPI_InitFinalize mpi_initialization(
    argc, argv, testing_max_num_threads());


  try
    {
      {
        TrilinosWrappers::SparseMatrix v(14U, 14U, 3U);
        test(v);
      }
    }
  catch (const std::exception &exc)
    {
      std::cerr << std::endl
                << std::endl
                << "----------------------------------------------------"
                << std::endl;
      std::cerr << "Exception on processing: " << std::endl
                << exc.what() << std::endl
                << "Aborting!" << std::endl
                << "----------------------------------------------------"
                << std::endl;

      return 1;
    }
  catch (...)
    {
      std::cerr << std::endl
                << std::endl
                << "----------------------------------------------------"
                << std::endl;
      std::cerr << "Unknown exception!" << std::endl
                << "Aborting!" << std::endl
                << "----------------------------------------------------"
                << std::endl;
      return 1;
    };
}
