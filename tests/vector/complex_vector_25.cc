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


// check Vector::operator = (Scalar) with setting to zero

#include <deal.II/lac/vector.h>

#include <vector>

#include "../tests.h"


void
test(Vector<std::complex<double>> &v)
{
  // set some entries of the vector
  for (unsigned int i = 0; i < v.size(); ++i)
    if (i % 3 == 0)
      v(i) = std::complex<double>(i + 1., i + 2.);
  v.compress();

  // then clear it again and make sure the
  // vector is really empty
  const unsigned int sz = v.size();
  v                     = 0;
  AssertThrow(v.size() == sz, ExcInternalError());
  AssertThrow(v.l2_norm() == 0, ExcInternalError());

  deallog << "OK" << std::endl;
}


int
main()
{
  initlog();

  try
    {
      Vector<std::complex<double>> v(100);
      test(v);
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
