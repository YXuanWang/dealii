// ---------------------------------------------------------------------
//
// Copyright (C) 2004 - 2018 by the deal.II authors
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


// test SparseMatrix::iterator::operator-

#include <deal.II/lac/sparse_matrix.h>

#include "../tests.h"


void
test()
{
  SparsityPattern sp(5, 5, 3);
  for (unsigned int i = 0; i < 5; ++i)
    for (unsigned int j = 0; j < 5; ++j)
      if (((i + 2 * j + 1) % 3 == 0) || (i == j))
        sp.add(i, j);
  sp.compress();

  SparseMatrix<double> m(sp);

  for (unsigned int row = 0; row < sp.n_rows(); ++row)
    AssertThrow(m.begin(row) - m.begin(row) == 0, ExcInternalError());

  for (unsigned int row = 0; row < sp.n_rows(); ++row)
    AssertThrow(m.end(row) - m.begin(row) == (int)sp.row_length(row),
                ExcInternalError());
  for (unsigned int row = 0; row < sp.n_rows(); ++row)
    AssertThrow(m.begin(row) - m.end(row) == -(int)sp.row_length(row),
                ExcInternalError());

  {
    unsigned int counter = 0;
    for (unsigned int row = 0; row < sp.n_rows(); ++row)
      {
        AssertThrow(m.begin(row) - m.begin(0) == (int)counter,
                    ExcInternalError());
        AssertThrow(m.begin(0) - m.begin(row) == -(int)counter,
                    ExcInternalError());
        counter += sp.row_length(row);
      }
  }

  AssertThrow(m.begin() - m.begin(0) == 0, ExcInternalError());
  AssertThrow(m.begin(0) - m.begin() == 0, ExcInternalError());
  AssertThrow(m.end(sp.n_rows() - 1) - m.end() == 0, ExcInternalError());
  AssertThrow(m.end() - m.end(sp.n_rows() - 1) == 0, ExcInternalError());
  AssertThrow(m.end() - m.begin() == (int)sp.n_nonzero_elements(),
              ExcInternalError());
  AssertThrow(m.begin() - m.end() == -(int)sp.n_nonzero_elements(),
              ExcInternalError());

  deallog << "OK" << std::endl;
}



int
main()
{
  initlog();

  try
    {
      test();
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
