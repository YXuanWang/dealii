// ---------------------------------------------------------------------
//
// Copyright (C) 2018 - 2023 by the deal.II authors
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


// check that LinearAlgebra::distributed::Vector::reinit does not carry over any
// state that can lead to invalid memory access. In this test, the MPI
// communicator is deleted.


#include <deal.II/base/mpi.h>

#include <deal.II/lac/la_parallel_vector.h>
#include <deal.II/lac/la_parallel_vector.templates.h>
#include <deal.II/lac/vector_memory.h>
#include <deal.II/lac/vector_memory.templates.h>

#include "../tests.h"

template <typename VectorType>
void
do_test()
{
  IndexSet set(5);
  set.add_range(0, 5);

  VectorType v1, v2;

  {
    MPI_Comm communicator =
      Utilities::MPI::duplicate_communicator(MPI_COMM_WORLD);
    v1.reinit(set, communicator);
    deallog << "reinit: " << v1.size() << " ";

#ifdef DEAL_II_WITH_MPI
    MPI_Comm_free(&communicator);
#endif
  }

  {
    MPI_Comm communicator =
      Utilities::MPI::duplicate_communicator(MPI_COMM_WORLD);
    v2.reinit(set, communicator);
    v1.reinit(v2);
    deallog << v1.size() << " ";
    v1.reinit(v2);
    deallog << v1.size() << std::endl;

#ifdef DEAL_II_WITH_MPI
    MPI_Comm_free(&communicator);
#endif
  }

  {
    MPI_Comm communicator =
      Utilities::MPI::duplicate_communicator(MPI_COMM_WORLD);
    v2.reinit(set, communicator);
    v1 = v2;
    deallog << "assign " << v1.size() << " ";
    v1 = v2;
    deallog << v1.size() << std::endl;

#ifdef DEAL_II_WITH_MPI
    MPI_Comm_free(&communicator);
#endif
  }

  {
    MPI_Comm communicator =
      Utilities::MPI::duplicate_communicator(MPI_COMM_WORLD);
    GrowingVectorMemory<VectorType>            memory;
    typename VectorMemory<VectorType>::Pointer v3(memory);
    v1.reinit(set, communicator);
    v3->reinit(v1);
    deallog << "reinit pool " << v1.size() << " " << v3->size() << " ";

#ifdef DEAL_II_WITH_MPI
    MPI_Comm_free(&communicator);
#endif
  }

  {
    MPI_Comm communicator =
      Utilities::MPI::duplicate_communicator(MPI_COMM_WORLD);
    GrowingVectorMemory<VectorType>            memory;
    typename VectorMemory<VectorType>::Pointer v3(memory);
    v1.reinit(set, communicator);
    v3->reinit(v1);
    deallog << "reinit pool " << v3->size() << std::endl;

#ifdef DEAL_II_WITH_MPI
    MPI_Comm_free(&communicator);
#endif
  }
}

int
main(int argc, char **argv)
{
  Utilities::MPI::MPI_InitFinalize mpi_initialization(
    argc, argv, testing_max_num_threads());

  initlog();

  do_test<
    LinearAlgebra::distributed::Vector<double, dealii::MemorySpace::Default>>();
}
