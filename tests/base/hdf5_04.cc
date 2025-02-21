// ---------------------------------------------------------------------
//
// Copyright (C) 2015 - 2022 by the deal.II Authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE at
// the top level of the deal.II distribution.
//
// ---------------------------------------------------------------------

// Read/write tests with serial HDF5

#include <deal.II/base/hdf5.h>

#include <deal.II/lac/full_matrix.h>
#include <deal.II/lac/vector.h>

#include <boost/type_traits.hpp>

#include <numeric>

#include "../tests.h"

// This function initializes a container of Number type
template <template <class...> class Container, typename Number>
std::enable_if_t<std::is_same_v<Container<Number>, std::vector<Number>>,
                 Container<Number>>
initialize_container(std::vector<hsize_t> dimensions)
{
  return Container<Number>(std::accumulate(
    dimensions.begin(), dimensions.end(), 1, std::multiplies<int>()));
}

template <template <class...> class Container, typename Number>
std::enable_if_t<std::is_same_v<Container<Number>, Vector<Number>>,
                 Container<Number>>
initialize_container(std::vector<hsize_t> dimensions)
{
  return Container<Number>(std::accumulate(
    dimensions.begin(), dimensions.end(), 1, std::multiplies<int>()));
}

template <template <class...> class Container, typename Number>
std::enable_if_t<std::is_same_v<Container<Number>, FullMatrix<Number>>,
                 Container<Number>>
initialize_container(std::vector<hsize_t> dimensions)
{
  return FullMatrix<Number>(dimensions[0], dimensions[1]);
}

// This function calculates the sum of the elements in a container
template <template <class...> class Container, typename Number>
std::enable_if_t<std::is_same_v<Container<Number>, std::vector<Number>>, Number>
container_sum(Container<Number> data)
{
  return std::accumulate(data.begin(), data.end(), static_cast<Number>(0));
}

template <template <class...> class Container, typename Number>
std::enable_if_t<std::is_same_v<Container<Number>, Vector<Number>>, Number>
container_sum(Container<Number> data)
{
  return std::accumulate(data.begin(), data.end(), static_cast<Number>(0));
}

template <template <class...> class Container, typename Number>
std::enable_if_t<std::is_same_v<Container<Number>, FullMatrix<Number>>, Number>
container_sum(Container<Number> data)
{
  Number sum = 0;
  for (unsigned int row_idx = 0; row_idx < data.m(); ++row_idx)
    {
      for (unsigned int col_idx = 0; col_idx < data.n(); ++col_idx)
        {
          sum += data[row_idx][col_idx];
        }
    }
  return sum;
}


// This function is used to get a complex component in complex datasets
// If Number is scalar the function returns 1
// If Number is complex the function returns 1 + 1j
template <typename Number>
std::enable_if_t<!boost::is_complex<Number>::value, Number>
get_factor()
{
  return 1;
}

template <typename Number>
std::enable_if_t<boost::is_complex<Number>::value, Number>
get_factor()
{
  return static_cast<Number>(std::complex<float>(1, 1));
}

// This function assigns data to the elements of the container
template <template <class...> class Container, typename Number>
std::enable_if_t<std::is_same_v<Container<Number>, std::vector<Number>>, void>
assign_data(Container<Number> &data)
{
  for (unsigned int idx = 0; idx < data.size(); ++idx)
    {
      data[idx] = static_cast<Number>(idx) * get_factor<Number>();
    }
}

template <template <class...> class Container, typename Number>
std::enable_if_t<std::is_same_v<Container<Number>, Vector<Number>>, void>
assign_data(Container<Number> &data)
{
  for (unsigned int idx = 0; idx < data.size(); ++idx)
    {
      data[idx] = static_cast<Number>(idx) * get_factor<Number>();
    }
}

template <template <class...> class Container, typename Number>
std::enable_if_t<std::is_same_v<Container<Number>, FullMatrix<Number>>, void>
assign_data(Container<Number> &data)
{
  for (unsigned int row_idx = 0; row_idx < data.m(); ++row_idx)
    {
      for (unsigned int col_idx = 0; col_idx < data.n(); ++col_idx)
        {
          data[row_idx][col_idx] =
            static_cast<Number>(row_idx * data.n() + col_idx) *
            get_factor<Number>();
        }
    }
}

// This function tests parallel write and gets the group by reference
template <template <class...> class Container, typename Number>
void
write_test(HDF5::Group               &root_group,
           const std::vector<hsize_t> dataset_dimensions)
{
  AssertDimension(dataset_dimensions.size(), 2);

  std::string container_name;
  std::string type_name;

  if (std::is_same_v<Container<Number>, std::vector<Number>>)
    {
      container_name = std::string("std::vector");
    }
  else if (std::is_same_v<Container<Number>, FullMatrix<Number>>)
    {
      container_name = std::string("FullMatrix");
    }

  if (std::is_same_v<Number, float>)
    {
      type_name = std::string("float");
    }
  else if (std::is_same_v<Number, double>)
    {
      type_name = std::string("double");
    }
  else if (std::is_same_v<Number, std::complex<float>>)
    {
      type_name = std::string("std::complex<float>");
    }
  else if (std::is_same_v<Number, std::complex<double>>)
    {
      type_name = std::string("std::complex<double>");
    }
  else if (std::is_same_v<Number, int>)
    {
      type_name = std::string("int");
    }
  else if (std::is_same_v<Number, unsigned int>)
    {
      type_name = std::string("unsigned int");
    }

  deallog << "Write tests for " << container_name << '<' << type_name << '>'
          << " datasets" << std::endl;

  auto group = root_group.create_group(container_name + "<" + type_name + ">");

  {
    std::string dataset_name("dataset_1");
    auto        dataset =
      group.create_dataset<Number>(dataset_name, dataset_dimensions);
    dataset.set_query_io_mode(true);
    deallog << "Dimensions " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Write): " << dataset.get_dimensions()
            << std::endl;
    deallog << "Size " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Write): " << dataset.get_size()
            << std::endl;
    deallog << "Rank " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Write): " << dataset.get_rank()
            << std::endl;
    auto data = initialize_container<Container, Number>(dataset_dimensions);
    assign_data(data);
    dataset.write(data);
    deallog << "Sum " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Write): " << container_sum(data)
            << std::endl;
    std::cout << "IO mode " + dataset_name << ' ' << container_name << '<'
              << type_name << '>' << " (Write): " << dataset.get_io_mode()
              << std::endl;
    std::cout << "Local no collective cause " + dataset_name << ' '
              << container_name << '<' << type_name << '>'
              << " (Write): " << dataset.get_local_no_collective_cause()
              << std::endl;
    std::cout << "Global no collective cause " + dataset_name << ' '
              << container_name << '<' << type_name << '>'
              << " (Write): " << dataset.get_global_no_collective_cause()
              << std::endl;
  }

  {
    std::string dataset_name("dataset_2");
    auto        dataset =
      group.create_dataset<Number>(dataset_name, dataset_dimensions);
    dataset.set_query_io_mode(true);
    deallog << "Dimensions " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Write): " << dataset.get_dimensions()
            << std::endl;
    deallog << "Size " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Write): " << dataset.get_size()
            << std::endl;
    deallog << "Rank " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Write): " << dataset.get_rank()
            << std::endl;

    std::vector<hsize_t> coordinates_a = {0,
                                          0, // first point
                                          0,
                                          2, // second point
                                          3,
                                          4, // third point
                                          25,
                                          12}; // fourth point
    std::vector<Number>  data_a        = {2, 3, 5, 6};

    std::vector<hsize_t> coordinates_b = {5,
                                          0, // first point
                                          0,
                                          4, // second point
                                          5,
                                          4, // third point
                                          26,
                                          12}; // fourth point
    std::vector<Number>  data_b        = {9, 4, 7, 6};

    std::vector<hsize_t> coordinates_c = {5,
                                          7, // first point
                                          1,
                                          8, // second point
                                          5,
                                          5, // third point
                                          27,
                                          12, // fourth point
                                          28,
                                          29}; // fifth point
    std::vector<Number>  data_c        = {2, 3, 5, 6, 3};

    dataset.write_selection(data_a, coordinates_a);
    dataset.write_selection(data_b, coordinates_b);
    dataset.write_selection(data_c, coordinates_c);
    deallog << "Sum " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Write): "
            << container_sum(data_a) + container_sum(data_b) +
                 container_sum(data_c)
            << std::endl;
    std::cout << "IO mode " + dataset_name << ' ' << container_name << '<'
              << type_name << '>' << " (Write): " << dataset.get_io_mode()
              << std::endl;
    std::cout << "Local no collective cause " + dataset_name << ' '
              << container_name << '<' << type_name << '>'
              << " (Write): " << dataset.get_local_no_collective_cause()
              << std::endl;
    std::cout << "Global no collective cause " + dataset_name << ' '
              << container_name << '<' << type_name << '>'
              << " (Write): " << dataset.get_global_no_collective_cause()
              << std::endl;
  }

  {
    // In this dataset, data conversion is tested. The test is only performed
    // for float and double.
    if (std::is_same_v<Number, float>)
      {
        std::string dataset_name("dataset_3");
        auto        dataset =
          group.create_dataset<Number>(dataset_name, dataset_dimensions);
        dataset.set_query_io_mode(true);
        deallog << "Dimensions " + dataset_name << ' ' << container_name << '<'
                << type_name << '>' << " (Write): " << dataset.get_dimensions()
                << std::endl;
        deallog << "Size " + dataset_name << ' ' << container_name << '<'
                << type_name << '>' << " (Write): " << dataset.get_size()
                << std::endl;
        deallog << "Rank " + dataset_name << ' ' << container_name << '<'
                << type_name << '>' << " (Write): " << dataset.get_rank()
                << std::endl;
        auto data = initialize_container<Container, double>(dataset_dimensions);
        assign_data(data);
        dataset.write(data);
        deallog << "Sum " + dataset_name << ' ' << container_name << '<'
                << type_name << '>' << " (Write): " << container_sum(data)
                << std::endl;
      }
    else if (std::is_same_v<Number, double>)
      {
        std::string dataset_name("dataset_3");
        auto        dataset =
          group.create_dataset<Number>(dataset_name, dataset_dimensions);
        dataset.set_query_io_mode(true);
        auto data = initialize_container<Container, float>(dataset_dimensions);
        assign_data(data);
        dataset.write(data);
        deallog << "Sum " + dataset_name << ' ' << container_name << '<'
                << type_name << '>' << " (Write): " << container_sum(data)
                << std::endl;
        std::cout << "IO mode " + dataset_name << ' ' << container_name << '<'
                  << type_name << '>' << " (Write): " << dataset.get_io_mode()
                  << std::endl;
        std::cout << "Local no collective cause " + dataset_name << ' '
                  << container_name << '<' << type_name << '>'
                  << " (Write): " << dataset.get_local_no_collective_cause()
                  << std::endl;
        std::cout << "Global no collective cause " + dataset_name << ' '
                  << container_name << '<' << type_name << '>'
                  << " (Write): " << dataset.get_global_no_collective_cause()
                  << std::endl;
      }
  }

  {
    std::string dataset_name("dataset_4");
    auto        dataset =
      group.create_dataset<Number>(dataset_name, dataset_dimensions);
    dataset.set_query_io_mode(true);
    deallog << "Dimensions " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Write): " << dataset.get_dimensions()
            << std::endl;
    deallog << "Size " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Write): " << dataset.get_size()
            << std::endl;
    deallog << "Rank " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Write): " << dataset.get_rank()
            << std::endl;
    const std::vector<hsize_t> hyperslab_dimensions_a = {2, 5};
    const std::vector<hsize_t> hyperslab_offset_a     = {0, 0};
    auto                       hyperslab_data_a =
      initialize_container<Container, Number>(hyperslab_dimensions_a);
    assign_data(hyperslab_data_a);
    const std::vector<hsize_t> hyperslab_dimensions_b = {1, 4};
    const std::vector<hsize_t> hyperslab_offset_b     = {2, 0};
    auto                       hyperslab_data_b =
      initialize_container<Container, Number>(hyperslab_dimensions_b);
    assign_data(hyperslab_data_b);
    dataset.write_hyperslab(hyperslab_data_a,
                            hyperslab_offset_a,
                            hyperslab_dimensions_a);
    dataset.write_hyperslab(hyperslab_data_b,
                            hyperslab_offset_b,
                            hyperslab_dimensions_b);

    // Now let's do a second write operation to the dataset using the complex
    // version of write_hyperslab()
    const std::vector<hsize_t> hyperslab_dimensions_c = {6, 8};
    const std::vector<hsize_t> hyperslab_offset_c     = {4, 0};
    const std::vector<hsize_t> hyperslab_stride_c     = {4, 3};
    const std::vector<hsize_t> hyperslab_count_c      = {2, 4};
    const std::vector<hsize_t> hyperslab_block_c      = {3, 2};
    auto                       hyperslab_data_c =
      initialize_container<Container, Number>(hyperslab_dimensions_c);
    assign_data(hyperslab_data_c);
    dataset.write_hyperslab(hyperslab_data_c,
                            hyperslab_dimensions_c,
                            hyperslab_offset_c,
                            hyperslab_stride_c,
                            hyperslab_count_c,
                            hyperslab_block_c);

    deallog << "Sum " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Write): "
            << container_sum(hyperslab_data_a) +
                 container_sum(hyperslab_data_b) +
                 container_sum(hyperslab_data_c)
            << std::endl;
    deallog << "Hyperslab_a sum " + dataset_name << ' ' << container_name << '<'
            << type_name << '>'
            << " (Write): " << container_sum(hyperslab_data_a) << std::endl;
    deallog << "Hyperslab_b sum " + dataset_name << ' ' << container_name << '<'
            << type_name << '>'
            << " (Write): " << container_sum(hyperslab_data_b) << std::endl;
    std::cout << "IO mode " + dataset_name << ' ' << container_name << '<'
              << type_name << '>' << " (Write): " << dataset.get_io_mode()
              << std::endl;
    std::cout << "Local no collective cause " + dataset_name << ' '
              << container_name << '<' << type_name << '>'
              << " (Write): " << dataset.get_local_no_collective_cause()
              << std::endl;
    std::cout << "Global no collective cause " + dataset_name << ' '
              << container_name << '<' << type_name << '>'
              << " (Write): " << dataset.get_global_no_collective_cause()
              << std::endl;
  }

  {
    std::string dataset_name("dataset_5");
    auto data = initialize_container<Container, Number>(dataset_dimensions);
    assign_data(data);
    group.write_dataset(dataset_name, data);
    deallog << "Sum " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Write): " << container_sum(data)
            << std::endl;
  }
}


// This function tests parallel read. Unlike write_test, this functions gets a
// copy of the group
template <template <class...> class Container, typename Number>
void
read_test(HDF5::Group root_group)
{
  std::string container_name;
  std::string type_name;

  if (std::is_same_v<Container<Number>, std::vector<Number>>)
    {
      container_name = std::string("std::vector");
    }
  else if (std::is_same_v<Container<Number>, FullMatrix<Number>>)
    {
      container_name = std::string("FullMatrix");
    }

  if (std::is_same_v<Number, float>)
    {
      type_name = std::string("float");
    }
  else if (std::is_same_v<Number, double>)
    {
      type_name = std::string("double");
    }
  else if (std::is_same_v<Number, std::complex<float>>)
    {
      type_name = std::string("std::complex<float>");
    }
  else if (std::is_same_v<Number, std::complex<double>>)
    {
      type_name = std::string("std::complex<double>");
    }
  else if (std::is_same_v<Number, int>)
    {
      type_name = std::string("int");
    }
  else if (std::is_same_v<Number, unsigned int>)
    {
      type_name = std::string("unsigned int");
    }

  deallog << "Read tests for " << container_name << '<' << type_name << '>'
          << " datasets" << std::endl;

  auto group = root_group.open_group(container_name + "<" + type_name + ">");

  {
    std::string dataset_name("dataset_1");
    auto        dataset = group.open_dataset(dataset_name);
    dataset.set_query_io_mode(true);
    deallog << "Dimensions " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Read): " << dataset.get_dimensions()
            << std::endl;
    deallog << "Size " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Read): " << dataset.get_size()
            << std::endl;
    deallog << "Rank " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Read): " << dataset.get_rank()
            << std::endl;
    Container<Number> data = dataset.read<Container<Number>>();
    deallog << "Sum " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Read): " << container_sum(data)
            << std::endl;
    std::cout << "IO mode " + dataset_name << ' ' << container_name << '<'
              << type_name << '>' << " (Read): " << dataset.get_io_mode()
              << std::endl;
    std::cout << "Local no collective cause " + dataset_name << ' '
              << container_name << '<' << type_name << '>'
              << " (Read): " << dataset.get_local_no_collective_cause()
              << std::endl;
    std::cout << "Global no collective cause " + dataset_name << ' '
              << container_name << '<' << type_name << '>'
              << " (Read): " << dataset.get_global_no_collective_cause()
              << std::endl;
  }

  {
    std::string dataset_name("dataset_2");
    auto        dataset = group.open_dataset(dataset_name);
    dataset.set_query_io_mode(true);
    deallog << "Dimensions " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Read): " << dataset.get_dimensions()
            << std::endl;
    deallog << "Size " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Read): " << dataset.get_size()
            << std::endl;
    deallog << "Rank " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Read): " << dataset.get_rank()
            << std::endl;
    {
      Container<Number> data = dataset.read<Container<Number>>();
      deallog << "Sum " + dataset_name << ' ' << container_name << '<'
              << type_name << '>' << " (Read): " << container_sum(data)
              << std::endl;
      std::cout << "IO mode " + dataset_name << ' ' << container_name << '<'
                << type_name << '>' << " (Read): " << dataset.get_io_mode()
                << std::endl;
      std::cout << "Local no collective cause " + dataset_name << ' '
                << container_name << '<' << type_name << '>'
                << " (Read): " << dataset.get_local_no_collective_cause()
                << std::endl;
      std::cout << "Global no collective cause " + dataset_name << ' '
                << container_name << '<' << type_name << '>'
                << " (Read): " << dataset.get_global_no_collective_cause()
                << std::endl;
    }

    {
      std::vector<hsize_t> coordinates_a = {0,
                                            0, // first point
                                            0,
                                            2, // second point
                                            3,
                                            4, // third point
                                            25,
                                            12}; // fourth point
      auto                 data_a =
        dataset.template read_selection<std::vector<Number>>(coordinates_a);
      deallog << "Selection " + dataset_name << ' ' << container_name << '<'
              << type_name << '>' << " (Read): " << data_a[0] << ", "
              << data_a[1] << ", " << data_a[2] << ", " << data_a[3]
              << std::endl;
      std::cout << "IO mode " + dataset_name << ' ' << container_name << '<'
                << type_name << '>' << " (Read): " << dataset.get_io_mode()
                << std::endl;
      std::cout << "Local no collective cause " + dataset_name << ' '
                << container_name << '<' << type_name << '>'
                << " (Read): " << dataset.get_local_no_collective_cause()
                << std::endl;
      std::cout << "Global no collective cause " + dataset_name << ' '
                << container_name << '<' << type_name << '>'
                << " (Read): " << dataset.get_global_no_collective_cause()
                << std::endl;
    }
  }

  {
    // In this test data conversion is tested. The dataset only exists for
    // float and double.
    if (std::is_same_v<Number, float> || std::is_same_v<Number, double>)
      {
        std::string dataset_name("dataset_3");
        auto        dataset = group.open_dataset(dataset_name);
        dataset.set_query_io_mode(true);
        deallog << "Dimensions " + dataset_name << ' ' << container_name << '<'
                << type_name << '>' << " (Read): " << dataset.get_dimensions()
                << std::endl;
        deallog << "Size " + dataset_name << ' ' << container_name << '<'
                << type_name << '>' << " (Read): " << dataset.get_size()
                << std::endl;
        deallog << "Rank " + dataset_name << ' ' << container_name << '<'
                << type_name << '>' << " (Read): " << dataset.get_rank()
                << std::endl;
        Container<Number> data = dataset.read<Container<Number>>();
        deallog << "Sum " + dataset_name << ' ' << container_name << '<'
                << type_name << '>' << " (Read): " << container_sum(data)
                << std::endl;
        std::cout << "IO mode " + dataset_name << ' ' << container_name << '<'
                  << type_name << '>' << " (Read): " << dataset.get_io_mode()
                  << std::endl;
        std::cout << "Local no collective cause " + dataset_name << ' '
                  << container_name << '<' << type_name << '>'
                  << " (Read): " << dataset.get_local_no_collective_cause()
                  << std::endl;
        std::cout << "Global no collective cause " + dataset_name << ' '
                  << container_name << '<' << type_name << '>'
                  << " (Read): " << dataset.get_global_no_collective_cause()
                  << std::endl;
      }
  }

  {
    std::string dataset_name("dataset_4");
    auto        dataset = group.open_dataset(dataset_name);
    dataset.set_query_io_mode(true);
    deallog << "Dimensions " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Read): " << dataset.get_dimensions()
            << std::endl;
    deallog << "Size " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Read): " << dataset.get_size()
            << std::endl;
    deallog << "Rank " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Read): " << dataset.get_rank()
            << std::endl;
    {
      Container<Number> data = dataset.read<Container<Number>>();
      deallog << "Sum " + dataset_name << ' ' << container_name << '<'
              << type_name << '>' << " (Read): " << container_sum(data)
              << std::endl;
      std::cout << "IO mode " + dataset_name << ' ' << container_name << '<'
                << type_name << '>' << " (Read): " << dataset.get_io_mode()
                << std::endl;
      std::cout << "Local no collective cause " + dataset_name << ' '
                << container_name << '<' << type_name << '>'
                << " (Read): " << dataset.get_local_no_collective_cause()
                << std::endl;
      std::cout << "Global no collective cause " + dataset_name << ' '
                << container_name << '<' << type_name << '>'
                << " (Read): " << dataset.get_global_no_collective_cause()
                << std::endl;
    }

    {
      const std::vector<hsize_t> hyperslab_offset_a = {0, 0};
      const std::vector<hsize_t> hyperslab_count_a  = {2, 5};
      auto                       data_a =
        dataset.read_hyperslab<Container<Number>>(hyperslab_offset_a,
                                                  hyperslab_count_a);
      deallog << "Hyperslab_a sum " + dataset_name << ' ' << container_name
              << '<' << type_name << '>' << " (Read): " << container_sum(data_a)
              << std::endl;
      std::cout << "IO mode " + dataset_name << ' ' << container_name << '<'
                << type_name << '>' << " (Read): " << dataset.get_io_mode()
                << std::endl;
      std::cout << "Local no collective cause " + dataset_name << ' '
                << container_name << '<' << type_name << '>'
                << " (Read): " << dataset.get_local_no_collective_cause()
                << std::endl;
      std::cout << "Global no collective cause " + dataset_name << ' '
                << container_name << '<' << type_name << '>'
                << " (Read): " << dataset.get_global_no_collective_cause()
                << std::endl;
    }

    {
      const std::vector<hsize_t> hyperslab_offset_b = {2, 0};
      const std::vector<hsize_t> hyperslab_count_b  = {1, 4};
      Container<Number>          data_b;
      data_b = dataset.read_hyperslab<Container<Number>>(hyperslab_offset_b,
                                                         hyperslab_count_b);
      deallog << "Hyperslab_b sum " + dataset_name << ' ' << container_name
              << '<' << type_name << '>' << " (Read): " << container_sum(data_b)
              << std::endl;
      std::cout << "IO mode " + dataset_name << ' ' << container_name << '<'
                << type_name << '>' << " (Read): " << dataset.get_io_mode()
                << std::endl;
      std::cout << "Local no collective cause " + dataset_name << ' '
                << container_name << '<' << type_name << '>'
                << " (Read): " << dataset.get_local_no_collective_cause()
                << std::endl;
      std::cout << "Global no collective cause " + dataset_name << ' '
                << container_name << '<' << type_name << '>'
                << " (Read): " << dataset.get_global_no_collective_cause()
                << std::endl;
    }
  }

  {
    std::string dataset_name("dataset_5");
    auto        dataset = group.open_dataset(dataset_name);
    dataset.set_query_io_mode(true);
    deallog << "Dimensions " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Read): " << dataset.get_dimensions()
            << std::endl;
    deallog << "Size " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Read): " << dataset.get_size()
            << std::endl;
    deallog << "Rank " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Read): " << dataset.get_rank()
            << std::endl;
    Container<Number> data = dataset.read<Container<Number>>();
    deallog << "Sum " + dataset_name << ' ' << container_name << '<'
            << type_name << '>' << " (Read): " << container_sum(data)
            << std::endl;
    std::cout << "IO mode " + dataset_name << ' ' << container_name << '<'
              << type_name << '>' << " (Read): " << dataset.get_io_mode()
              << std::endl;
    std::cout << "Local no collective cause " + dataset_name << ' '
              << container_name << '<' << type_name << '>'
              << " (Read): " << dataset.get_local_no_collective_cause()
              << std::endl;
    std::cout << "Global no collective cause " + dataset_name << ' '
              << container_name << '<' << type_name << '>'
              << " (Read): " << dataset.get_global_no_collective_cause()
              << std::endl;
  }
}


int
main()
{
  initlog();

  try
    {
      std::string                filename           = "test.h5";
      const std::vector<hsize_t> dataset_dimensions = {50, 30};

      {
        HDF5::File data_file(filename, HDF5::File::FileAccessMode::create);

        write_test<std::vector, float>(data_file, dataset_dimensions);
        write_test<std::vector, double>(data_file, dataset_dimensions);

#ifdef DEAL_II_WITH_COMPLEX_VALUES
        write_test<std::vector, std::complex<float>>(data_file,
                                                     dataset_dimensions);
        write_test<std::vector, std::complex<double>>(data_file,
                                                      dataset_dimensions);
#endif

        write_test<std::vector, int>(data_file, dataset_dimensions);
        write_test<std::vector, unsigned int>(data_file, dataset_dimensions);
        write_test<FullMatrix, float>(data_file, dataset_dimensions);
        write_test<FullMatrix, double>(data_file, dataset_dimensions);

#ifdef DEAL_II_WITH_COMPLEX_VALUES
        write_test<FullMatrix, std::complex<float>>(data_file,
                                                    dataset_dimensions);
        write_test<FullMatrix, std::complex<double>>(data_file,
                                                     dataset_dimensions);
#endif

        write_test<Vector, float>(data_file, dataset_dimensions);
        write_test<Vector, double>(data_file, dataset_dimensions);

#ifdef DEAL_II_WITH_COMPLEX_VALUES
        write_test<Vector, std::complex<float>>(data_file, dataset_dimensions);
        write_test<Vector, std::complex<double>>(data_file, dataset_dimensions);
#endif
      }

      {
        HDF5::File data_file(filename, HDF5::File::FileAccessMode::open);
        read_test<std::vector, float>(data_file);
        read_test<std::vector, double>(data_file);

#ifdef DEAL_II_WITH_COMPLEX_VALUES
        read_test<std::vector, std::complex<float>>(data_file);
        read_test<std::vector, std::complex<double>>(data_file);
#endif

        read_test<std::vector, int>(data_file);
        read_test<std::vector, unsigned int>(data_file);
        read_test<FullMatrix, float>(data_file);
        read_test<FullMatrix, double>(data_file);

#ifdef DEAL_II_WITH_COMPLEX_VALUES
        read_test<FullMatrix, std::complex<float>>(data_file);
        read_test<FullMatrix, std::complex<double>>(data_file);
#endif

        read_test<Vector, float>(data_file);
        read_test<Vector, double>(data_file);

#ifdef DEAL_II_WITH_COMPLEX_VALUES
        read_test<Vector, std::complex<float>>(data_file);
        read_test<Vector, std::complex<double>>(data_file);
#endif
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
