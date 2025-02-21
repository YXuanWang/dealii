// ---------------------------------------------------------------------
//
// Copyright (C) 1998 - 2018 by the deal.II authors
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

#include <deal.II/base/job_identifier.h>

#include <ctime>

#ifdef DEAL_II_HAVE_UNISTD_H
#  include <unistd.h>
#endif

DEAL_II_NAMESPACE_OPEN


const JobIdentifier &
JobIdentifier::get_dealjobid()
{
  static JobIdentifier dealjobid;
  return dealjobid;
}



JobIdentifier::JobIdentifier()
{
  time_t t = std::time(nullptr);
  id       = std::string("JobId ");

#if defined(DEAL_II_HAVE_UNISTD_H) && defined(DEAL_II_HAVE_GETHOSTNAME)
  char name[100];
  gethostname(name, 99);
  id += std::string(name) + std::string(" ");
#else
  id += std::string("unknown ");
#endif

  id += std::string(std::ctime(&t));
}


std::string
JobIdentifier::operator()() const
{
  return id;
}


std::string
JobIdentifier::base_name(const std::string &filename)
{
  std::string            name(filename);
  std::string::size_type pos;
  pos = name.rfind('/');
  if (pos != std::string::npos)
    name.erase(0, pos + 1);
  pos = name.rfind('.');
  if (pos != std::string::npos)
    name.erase(pos, name.size());
  return name;
}



DEAL_II_NAMESPACE_CLOSE
