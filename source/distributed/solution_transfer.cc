// ---------------------------------------------------------------------
//
// Copyright (C) 2009 - 2023 by the deal.II authors
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


#include <deal.II/base/config.h>

#ifdef DEAL_II_WITH_P4EST

#  include <deal.II/distributed/solution_transfer.h>
#  include <deal.II/distributed/tria.h>

#  include <deal.II/dofs/dof_accessor.h>
#  include <deal.II/dofs/dof_tools.h>

#  include <deal.II/grid/tria_accessor.h>
#  include <deal.II/grid/tria_iterator.h>

#  include <deal.II/lac/block_vector.h>
#  include <deal.II/lac/la_parallel_block_vector.h>
#  include <deal.II/lac/la_parallel_vector.h>
#  include <deal.II/lac/petsc_block_vector.h>
#  include <deal.II/lac/petsc_vector.h>
#  include <deal.II/lac/trilinos_parallel_block_vector.h>
#  include <deal.II/lac/trilinos_vector.h>
#  include <deal.II/lac/vector.h>

#  include <functional>
#  include <numeric>


DEAL_II_NAMESPACE_OPEN


namespace
{
  /**
   * Optimized pack function for values assigned on degrees of freedom.
   *
   * Given that the elements of @p dof_values are stored in consecutive
   * locations, we can just memcpy them. Since floating point values don't
   * compress well, we also waive the compression that the default
   * Utilities::pack() and Utilities::unpack() functions offer.
   */
  template <typename value_type>
  std::vector<char>
  pack_dof_values(std::vector<Vector<value_type>> &dof_values,
                  const unsigned int               dofs_per_cell)
  {
    for (const auto &values : dof_values)
      {
        AssertDimension(values.size(), dofs_per_cell);
        (void)values;
      }

    const std::size_t bytes_per_entry = sizeof(value_type) * dofs_per_cell;

    std::vector<char> buffer(dof_values.size() * bytes_per_entry);
    for (unsigned int i = 0; i < dof_values.size(); ++i)
      std::memcpy(&buffer[i * bytes_per_entry],
                  &dof_values[i](0),
                  bytes_per_entry);

    return buffer;
  }



  /**
   * Optimized unpack function for values assigned on degrees of freedom.
   */
  template <typename value_type>
  std::vector<Vector<value_type>>
  unpack_dof_values(
    const boost::iterator_range<std::vector<char>::const_iterator> &data_range,
    const unsigned int dofs_per_cell)
  {
    const std::size_t  bytes_per_entry = sizeof(value_type) * dofs_per_cell;
    const unsigned int n_elements      = data_range.size() / bytes_per_entry;

    Assert((data_range.size() % bytes_per_entry == 0), ExcInternalError());

    std::vector<Vector<value_type>> unpacked_data;
    unpacked_data.reserve(n_elements);
    for (unsigned int i = 0; i < n_elements; ++i)
      {
        Vector<value_type> dof_values(dofs_per_cell);
        std::memcpy(&dof_values(0),
                    &(*std::next(data_range.begin(), i * bytes_per_entry)),
                    bytes_per_entry);
        unpacked_data.emplace_back(std::move(dof_values));
      }

    return unpacked_data;
  }
} // namespace



namespace parallel
{
  namespace distributed
  {
    template <int dim, typename VectorType, int spacedim>
    SolutionTransfer<dim, VectorType, spacedim>::SolutionTransfer(
      const DoFHandler<dim, spacedim> &dof,
      const bool                       average_values)
      : dof_handler(&dof, typeid(*this).name())
      , average_values(average_values)
      , handle(numbers::invalid_unsigned_int)
    {
      Assert(
        (dynamic_cast<
           const parallel::DistributedTriangulationBase<dim, spacedim> *>(
           &dof_handler->get_triangulation()) != nullptr),
        ExcMessage(
          "parallel::distributed::SolutionTransfer requires a parallel::distributed::Triangulation object."));
    }



    template <int dim, typename VectorType, int spacedim>
    void
    SolutionTransfer<dim, VectorType, spacedim>::
      prepare_for_coarsening_and_refinement(
        const std::vector<const VectorType *> &all_in)
    {
      for (unsigned int i = 0; i < all_in.size(); ++i)
        Assert(all_in[i]->size() == dof_handler->n_dofs(),
               ExcDimensionMismatch(all_in[i]->size(), dof_handler->n_dofs()));

      input_vectors = all_in;
      register_data_attach();
    }



    template <int dim, typename VectorType, int spacedim>
    void
    SolutionTransfer<dim, VectorType, spacedim>::register_data_attach()
    {
      // TODO: casting away constness is bad
      parallel::DistributedTriangulationBase<dim, spacedim> *tria =
        (dynamic_cast<parallel::DistributedTriangulationBase<dim, spacedim> *>(
          const_cast<dealii::Triangulation<dim, spacedim> *>(
            &dof_handler->get_triangulation())));
      Assert(tria != nullptr, ExcInternalError());

      Assert(handle == numbers::invalid_unsigned_int,
             ExcMessage("You can only add one solution per "
                        "SolutionTransfer object."));

      handle = tria->register_data_attach(
        [this](
          const typename Triangulation<dim, spacedim>::cell_iterator &cell_,
          const CellStatus                                            status) {
          return this->pack_callback(cell_, status);
        },
        /*returns_variable_size_data=*/dof_handler->has_hp_capabilities());
    }



    template <int dim, typename VectorType, int spacedim>
    void
    SolutionTransfer<dim, VectorType, spacedim>::
      prepare_for_coarsening_and_refinement(const VectorType &in)
    {
      std::vector<const VectorType *> all_in(1, &in);
      prepare_for_coarsening_and_refinement(all_in);
    }



    template <int dim, typename VectorType, int spacedim>
    void
    SolutionTransfer<dim, VectorType, spacedim>::prepare_for_serialization(
      const VectorType &in)
    {
      std::vector<const VectorType *> all_in(1, &in);
      prepare_for_serialization(all_in);
    }



    template <int dim, typename VectorType, int spacedim>
    void
    SolutionTransfer<dim, VectorType, spacedim>::prepare_for_serialization(
      const std::vector<const VectorType *> &all_in)
    {
      prepare_for_coarsening_and_refinement(all_in);
    }



    template <int dim, typename VectorType, int spacedim>
    void
    SolutionTransfer<dim, VectorType, spacedim>::deserialize(VectorType &in)
    {
      std::vector<VectorType *> all_in(1, &in);
      deserialize(all_in);
    }



    template <int dim, typename VectorType, int spacedim>
    void
    SolutionTransfer<dim, VectorType, spacedim>::deserialize(
      std::vector<VectorType *> &all_in)
    {
      register_data_attach();

      // this makes interpolate() happy
      input_vectors.resize(all_in.size());

      interpolate(all_in);
    }


    template <int dim, typename VectorType, int spacedim>
    void
    SolutionTransfer<dim, VectorType, spacedim>::interpolate(
      std::vector<VectorType *> &all_out)
    {
      Assert(input_vectors.size() == all_out.size(),
             ExcDimensionMismatch(input_vectors.size(), all_out.size()));
      for (unsigned int i = 0; i < all_out.size(); ++i)
        Assert(all_out[i]->size() == dof_handler->n_dofs(),
               ExcDimensionMismatch(all_out[i]->size(), dof_handler->n_dofs()));

      // TODO: casting away constness is bad
      parallel::DistributedTriangulationBase<dim, spacedim> *tria =
        (dynamic_cast<parallel::DistributedTriangulationBase<dim, spacedim> *>(
          const_cast<dealii::Triangulation<dim, spacedim> *>(
            &dof_handler->get_triangulation())));
      Assert(tria != nullptr, ExcInternalError());

      if (average_values)
        for (auto *const vec : all_out)
          *vec = 0.0;

      VectorType valence;

      // initialize valence vector only if we need to average
      if (average_values)
        valence.reinit(*all_out[0]);

      tria->notify_ready_to_unpack(
        handle,
        [this, &all_out, &valence](
          const typename Triangulation<dim, spacedim>::cell_iterator &cell_,
          const CellStatus                                            status,
          const boost::iterator_range<std::vector<char>::const_iterator>
            &data_range) {
          this->unpack_callback(cell_, status, data_range, all_out, valence);
        });

      if (average_values)
        {
          // finalize valence: compress and invert
          using Number = typename VectorType::value_type;
          valence.compress(VectorOperation::add);
          for (const auto i : valence.locally_owned_elements())
            valence[i] = (static_cast<Number>(valence[i]) == Number() ?
                            Number() :
                            (Number(1.0) / static_cast<Number>(valence[i])));
          valence.compress(VectorOperation::insert);

          for (auto *const vec : all_out)
            {
              // compress and weight with valence
              vec->compress(VectorOperation::add);
              vec->scale(valence);
            }
        }
      else
        {
          for (auto *const vec : all_out)
            vec->compress(VectorOperation::insert);
        }

      input_vectors.clear();
      handle = numbers::invalid_unsigned_int;
    }



    template <int dim, typename VectorType, int spacedim>
    void
    SolutionTransfer<dim, VectorType, spacedim>::interpolate(VectorType &out)
    {
      std::vector<VectorType *> all_out(1, &out);
      interpolate(all_out);
    }



    template <int dim, typename VectorType, int spacedim>
    std::vector<char>
    SolutionTransfer<dim, VectorType, spacedim>::pack_callback(
      const typename Triangulation<dim, spacedim>::cell_iterator &cell_,
      const CellStatus                                            status)
    {
      typename DoFHandler<dim, spacedim>::cell_iterator cell(*cell_,
                                                             dof_handler);

      // create buffer for each individual object
      std::vector<::dealii::Vector<typename VectorType::value_type>> dof_values(
        input_vectors.size());

      unsigned int fe_index = 0;
      if (dof_handler->has_hp_capabilities())
        {
          switch (status)
            {
              case CellStatus::cell_will_persist:
              case CellStatus::cell_will_be_refined:
                {
                  fe_index = cell->future_fe_index();
                  break;
                }

              case CellStatus::children_will_be_coarsened:
                {
                  // In case of coarsening, we need to find a suitable FE index
                  // for the parent cell. We choose the 'least dominant fe'
                  // on all children from the associated FECollection.
#  ifdef DEBUG
                  for (const auto &child : cell->child_iterators())
                    Assert(child->is_active() && child->coarsen_flag_set(),
                           typename dealii::Triangulation<
                             dim>::ExcInconsistentCoarseningFlags());
#  endif

                  fe_index = dealii::internal::hp::DoFHandlerImplementation::
                    dominated_future_fe_on_children<dim, spacedim>(cell);
                  break;
                }

              default:
                Assert(false, ExcInternalError());
                break;
            }
        }

      const unsigned int dofs_per_cell =
        dof_handler->get_fe(fe_index).n_dofs_per_cell();

      if (dofs_per_cell == 0)
        return std::vector<char>(); // nothing to do for FE_Nothing

      auto it_input  = input_vectors.cbegin();
      auto it_output = dof_values.begin();
      for (; it_input != input_vectors.cend(); ++it_input, ++it_output)
        {
          it_output->reinit(dofs_per_cell);
          cell->get_interpolated_dof_values(*(*it_input), *it_output, fe_index);
        }

      return pack_dof_values<typename VectorType::value_type>(dof_values,
                                                              dofs_per_cell);
    }



    template <int dim, typename VectorType, int spacedim>
    void
    SolutionTransfer<dim, VectorType, spacedim>::unpack_callback(
      const typename Triangulation<dim, spacedim>::cell_iterator &cell_,
      const CellStatus                                            status,
      const boost::iterator_range<std::vector<char>::const_iterator>
                                &data_range,
      std::vector<VectorType *> &all_out,
      VectorType                &valence)
    {
      typename DoFHandler<dim, spacedim>::cell_iterator cell(*cell_,
                                                             dof_handler);

      unsigned int fe_index = 0;
      if (dof_handler->has_hp_capabilities())
        {
          switch (status)
            {
              case CellStatus::cell_will_persist:
              case CellStatus::children_will_be_coarsened:
                {
                  fe_index = cell->active_fe_index();
                  break;
                }

              case CellStatus::cell_will_be_refined:
                {
                  // After refinement, this particular cell is no longer active,
                  // and its children have inherited its FE index. However, to
                  // unpack the data on the old cell, we need to recover its FE
                  // index from one of the children. Just to be sure, we also
                  // check if all children have the same FE index.
                  fe_index = cell->child(0)->active_fe_index();
                  for (unsigned int child_index = 1;
                       child_index < cell->n_children();
                       ++child_index)
                    Assert(cell->child(child_index)->active_fe_index() ==
                             fe_index,
                           ExcInternalError());
                  break;
                }

              default:
                Assert(false, ExcInternalError());
                break;
            }
        }

      const unsigned int dofs_per_cell =
        dof_handler->get_fe(fe_index).n_dofs_per_cell();

      if (dofs_per_cell == 0)
        return; // nothing to do for FE_Nothing

      const std::vector<::dealii::Vector<typename VectorType::value_type>>
        dof_values =
          unpack_dof_values<typename VectorType::value_type>(data_range,
                                                             dofs_per_cell);

      // check if sizes match
      Assert(dof_values.size() == all_out.size(), ExcInternalError());

      // check if we have enough dofs provided by the FE object
      // to interpolate the transferred data correctly
      for (auto it_dof_values = dof_values.begin();
           it_dof_values != dof_values.end();
           ++it_dof_values)
        Assert(
          dofs_per_cell == it_dof_values->size(),
          ExcMessage(
            "The transferred data was packed with a different number of dofs than the "
            "currently registered FE object assigned to the DoFHandler has."));

      // distribute data for each registered vector on mesh
      auto it_input  = dof_values.cbegin();
      auto it_output = all_out.begin();
      for (; it_input != dof_values.cend(); ++it_input, ++it_output)
        if (average_values)
          cell->distribute_local_to_global_by_interpolation(*it_input,
                                                            *(*it_output),
                                                            fe_index);
        else
          cell->set_dof_values_by_interpolation(*it_input,
                                                *(*it_output),
                                                fe_index,
                                                true);

      if (average_values)
        {
          // compute valence vector if averaging should be performed
          Vector<typename VectorType::value_type> ones(dofs_per_cell);
          ones = 1.0;
          cell->distribute_local_to_global_by_interpolation(ones,
                                                            valence,
                                                            fe_index);
        }
    }
  } // namespace distributed
} // namespace parallel


// explicit instantiations
#  include "solution_transfer.inst"

DEAL_II_NAMESPACE_CLOSE

#endif
