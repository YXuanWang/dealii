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



// like _01, except that we use operator[] instead of operator*

#include <deal.II/lac/block_vector.h>

#include "../tests.h"


void
test()
{
  BlockVector<double> v(2, 1);
  v(0) = 1;
  v(1) = 2;

  // first check reading through a const
  // iterator
  {
    BlockVector<double>::const_iterator i = v.begin();
    AssertThrow(i[0] == 1, ExcInternalError());
    AssertThrow(i[1] == 2, ExcInternalError());
  }

  // same, but create iterator in a different
  // way
  {
    BlockVector<double>::const_iterator i =
      const_cast<const BlockVector<double> &>(v).begin();
    AssertThrow(i[0] == 1, ExcInternalError());
    AssertThrow(i[1] == 2, ExcInternalError());
  }

  // read through a read-write iterator
  {
    BlockVector<double>::iterator i = v.begin();
    AssertThrow(i[0] == 1, ExcInternalError());
    AssertThrow(i[1] == 2, ExcInternalError());
  }

  // write through a read-write iterator
  {
    BlockVector<double>::iterator i = v.begin();
    i[0]                            = 2;
    i[1]                            = 3;
  }

  // and read again
  {
    BlockVector<double>::iterator i = v.begin();
    AssertThrow(i[0] == 2, ExcInternalError());
    AssertThrow(i[1] == 3, ExcInternalError());
  }

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
