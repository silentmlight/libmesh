// rbOOmit: An implementation of the Certified Reduced Basis method.
// Copyright (C) 2009, 2010, 2015 David J. Knezevic

// This file is part of rbOOmit.

// rbOOmit is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// rbOOmit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include "libmesh/libmesh_config.h"
#if defined(LIBMESH_HAVE_CAPNPROTO)

//libMesh includes
#include "libmesh/rb_eim_evaluation.h"
#include "libmesh/string_to_enum.h"
#include "libmesh/elem.h"
#include "libmesh/mesh.h"
#include "libmesh/rb_data_deserialization.h"
#include "libmesh/transient_rb_theta_expansion.h"
#include "libmesh/transient_rb_evaluation.h"
#include "libmesh/rb_eim_evaluation.h"
#include "libmesh/rb_scm_evaluation.h"

// Cap'n'Proto includes
#include "capnp/serialize.h"

// C++ includes
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <fcntl.h>

namespace libMesh
{

namespace
{

/**
 * Helper function that reads either real or complex numbers, based on
 * the libMesh config options.
 */
template <typename T>
Number load_scalar_value(const T & value)
{
#ifdef LIBMESH_USE_COMPLEX_NUMBERS
  return Number(value.getReal(), value.getImag());
#else
  return value;
#endif
}

}

namespace RBDataDeserialization
{

// ---- RBEvaluationDeserialization (BEGIN) ----

RBEvaluationDeserialization::RBEvaluationDeserialization(RBEvaluation & rb_eval)
  :
  _rb_eval(rb_eval)
{}

RBEvaluationDeserialization::~RBEvaluationDeserialization()
{}

void RBEvaluationDeserialization::read_from_file(const std::string & path,
                                                 bool read_error_bound_data)
{
  LOG_SCOPE("read_from_file()", "RBEvaluationDeserialization");

  int fd = open(path.c_str(), O_RDONLY);
  if (!fd)
    libmesh_error_msg("Couldn't open the buffer file: " + path);

  // Turn off the limit to the amount of data we can read in
  capnp::ReaderOptions reader_options;
  reader_options.traversalLimitInWords = std::numeric_limits<uint64_t>::max();

  std::unique_ptr<capnp::StreamFdMessageReader> message;
  libmesh_try
    {
      message = libmesh_make_unique<capnp::StreamFdMessageReader>(fd, reader_options);
    }
  libmesh_catch(...)
    {
      libmesh_error_msg("Failed to open capnp buffer");
    }

#ifndef LIBMESH_USE_COMPLEX_NUMBERS
  RBData::RBEvaluationReal::Reader rb_eval_reader =
    message->getRoot<RBData::RBEvaluationReal>();
#else
  RBData::RBEvaluationComplex::Reader rb_eval_reader =
    message->getRoot<RBData::RBEvaluationComplex>();
#endif

  load_rb_evaluation_data(_rb_eval, rb_eval_reader, read_error_bound_data);

  int error = close(fd);
  if (error)
    libmesh_error_msg("Error closing a read-only file descriptor: " + path);
}

// ---- RBEvaluationDeserialization (END) ----


// ---- TransientRBEvaluationDeserialization (BEGIN) ----

TransientRBEvaluationDeserialization::
TransientRBEvaluationDeserialization(TransientRBEvaluation & trans_rb_eval) :
  _trans_rb_eval(trans_rb_eval)
{}

TransientRBEvaluationDeserialization::~TransientRBEvaluationDeserialization()
{}

void TransientRBEvaluationDeserialization::read_from_file(const std::string & path,
                                                          bool read_error_bound_data)
{
  LOG_SCOPE("read_from_file()", "TransientRBEvaluationDeserialization");

  int fd = open(path.c_str(), O_RDONLY);
  if (!fd)
    libmesh_error_msg("Couldn't open the buffer file: " + path);

  // Turn off the limit to the amount of data we can read in
  capnp::ReaderOptions reader_options;
  reader_options.traversalLimitInWords = std::numeric_limits<uint64_t>::max();

  std::unique_ptr<capnp::StreamFdMessageReader> message;
  libmesh_try
    {
      message = libmesh_make_unique<capnp::StreamFdMessageReader>(fd, reader_options);
    }
  libmesh_catch(...)
    {
      libmesh_error_msg("Failed to open capnp buffer");
    }

#ifndef LIBMESH_USE_COMPLEX_NUMBERS
  RBData::TransientRBEvaluationReal::Reader trans_rb_eval_reader =
    message->getRoot<RBData::TransientRBEvaluationReal>();
  RBData::RBEvaluationReal::Reader rb_eval_reader =
    trans_rb_eval_reader.getRbEvaluation();
#else
  RBData::TransientRBEvaluationComplex::Reader trans_rb_eval_reader =
    message->getRoot<RBData::TransientRBEvaluationComplex>();
  RBData::RBEvaluationComplex::Reader rb_eval_reader =
    trans_rb_eval_reader.getRbEvaluation();
#endif

  load_transient_rb_evaluation_data(_trans_rb_eval,
                                    rb_eval_reader,
                                    trans_rb_eval_reader,
                                    read_error_bound_data);

  int error = close(fd);
  if (error)
    libmesh_error_msg("Error closing a read-only file descriptor: " + path);
}

// ---- TransientRBEvaluationDeserialization (END) ----


// ---- RBEIMEvaluationDeserialization (BEGIN) ----

RBEIMEvaluationDeserialization::
RBEIMEvaluationDeserialization(RBEIMEvaluation & rb_eim_eval) :
  _rb_eim_eval(rb_eim_eval)
{}

RBEIMEvaluationDeserialization::~RBEIMEvaluationDeserialization()
{}

void RBEIMEvaluationDeserialization::read_from_file(const std::string & path)
{
  LOG_SCOPE("read_from_file()", "RBEIMEvaluationDeserialization");

  int fd = open(path.c_str(), O_RDONLY);
  if (!fd)
    libmesh_error_msg("Couldn't open the buffer file: " + path);

  // Turn off the limit to the amount of data we can read in
  capnp::ReaderOptions reader_options;
  reader_options.traversalLimitInWords = std::numeric_limits<uint64_t>::max();

  std::unique_ptr<capnp::StreamFdMessageReader> message;
  libmesh_try
    {
      message = libmesh_make_unique<capnp::StreamFdMessageReader>(fd, reader_options);
    }
  libmesh_catch(...)
    {
      libmesh_error_msg("Failed to open capnp buffer");
    }

#ifndef LIBMESH_USE_COMPLEX_NUMBERS
  RBData::RBEIMEvaluationReal::Reader rb_eim_eval_reader =
    message->getRoot<RBData::RBEIMEvaluationReal>();
  RBData::RBEvaluationReal::Reader rb_eval_reader =
    rb_eim_eval_reader.getRbEvaluation();
#else
  RBData::RBEIMEvaluationComplex::Reader rb_eim_eval_reader =
    message->getRoot<RBData::RBEIMEvaluationComplex>();
  RBData::RBEvaluationComplex::Reader rb_eval_reader =
    rb_eim_eval_reader.getRbEvaluation();
#endif

  load_rb_eim_evaluation_data(_rb_eim_eval,
                              rb_eval_reader,
                              rb_eim_eval_reader);

  int error = close(fd);
  if (error)
    libmesh_error_msg("Error closing a read-only file descriptor: " + path);
}

// ---- RBEIMEvaluationDeserialization (END) ----


// ---- RBSCMEvaluationDeserialization (BEGIN) ----
// RBSCMEvaluationDeserialization is only available if both SLEPC and
// GLPK are available.

#if defined(LIBMESH_HAVE_SLEPC) && (LIBMESH_HAVE_GLPK)

RBSCMEvaluationDeserialization::
RBSCMEvaluationDeserialization(RBSCMEvaluation & rb_scm_eval) :
  _rb_scm_eval(rb_scm_eval)
{}

RBSCMEvaluationDeserialization::~RBSCMEvaluationDeserialization()
{}

void RBSCMEvaluationDeserialization::read_from_file(const std::string & path)
{
  LOG_SCOPE("read_from_file()", "RBSCMEvaluationDeserialization");

  int fd = open(path.c_str(), O_RDONLY);
  if (!fd)
    libmesh_error_msg("Couldn't open the buffer file: " + path);

  // Turn off the limit to the amount of data we can read in
  capnp::ReaderOptions reader_options;
  reader_options.traversalLimitInWords = std::numeric_limits<uint64_t>::max();

  std::unique_ptr<capnp::StreamFdMessageReader> message;
  libmesh_try
    {
      message = libmesh_make_unique<capnp::StreamFdMessageReader>(fd, reader_options);
    }
  libmesh_catch(...)
    {
      libmesh_error_msg("Failed to open capnp buffer");
    }

  RBData::RBSCMEvaluation::Reader rb_scm_eval_reader =
    message->getRoot<RBData::RBSCMEvaluation>();

  load_rb_scm_evaluation_data(_rb_scm_eval,
                              rb_scm_eval_reader);

  int error = close(fd);
  if (error)
    libmesh_error_msg("Error closing a read-only file descriptor: " + path);
}

#endif // LIBMESH_HAVE_SLEPC && LIBMESH_HAVE_GLPK

// ---- RBSCMEvaluationDeserialization (END) ----


// ---- Helper functions for loading data from buffers (BEGIN) ----

void load_parameter_ranges(RBParametrized & rb_evaluation,
                           RBData::ParameterRanges::Reader & parameter_ranges,
                           RBData::DiscreteParameterList::Reader & discrete_parameters_list)
{
  // Continuous parameters
  RBParameters parameters_min;
  RBParameters parameters_max;
  {
    unsigned int n_parameter_ranges = parameter_ranges.getNames().size();

    for (unsigned int i=0; i<n_parameter_ranges; ++i)
      {
        std::string parameter_name = parameter_ranges.getNames()[i];
        Real min_value = parameter_ranges.getMinValues()[i];
        Real max_value = parameter_ranges.getMaxValues()[i];

        parameters_min.set_value(parameter_name, min_value);
        parameters_max.set_value(parameter_name, max_value);
      }
  }

  // Discrete parameters
  std::map<std::string, std::vector<Real>> discrete_parameter_values;
  {
    unsigned int n_discrete_parameters = discrete_parameters_list.getNames().size();

    for (unsigned int i=0; i<n_discrete_parameters; ++i)
      {
        std::string parameter_name = discrete_parameters_list.getNames()[i];

        auto value_list = discrete_parameters_list.getValues()[i];
        unsigned int n_values = value_list.size();
        std::vector<Real> values(n_values);
        for (unsigned int j=0; j<n_values; ++j)
          values[j] = value_list[j];

        discrete_parameter_values[parameter_name] = values;
      }
  }

  rb_evaluation.initialize_parameters(parameters_min,
                                      parameters_max,
                                      discrete_parameter_values);
}

template <typename RBEvaluationReaderNumber>
void load_rb_evaluation_data(RBEvaluation & rb_evaluation,
                             RBEvaluationReaderNumber & rb_evaluation_reader,
                             bool read_error_bound_data)
{
  // Set number of basis functions
  unsigned int n_bfs = rb_evaluation_reader.getNBfs();
  rb_evaluation.set_n_basis_functions(n_bfs);

  rb_evaluation.resize_data_structures(n_bfs, read_error_bound_data);

  auto parameter_ranges =
    rb_evaluation_reader.getParameterRanges();
  auto discrete_parameters_list =
    rb_evaluation_reader.getDiscreteParameters();

  load_parameter_ranges(rb_evaluation,
                        parameter_ranges,
                        discrete_parameters_list);

  const RBThetaExpansion & rb_theta_expansion = rb_evaluation.get_rb_theta_expansion();

  unsigned int n_F_terms = rb_theta_expansion.get_n_F_terms();
  unsigned int n_A_terms = rb_theta_expansion.get_n_A_terms();

  if (read_error_bound_data)
    {

      // Fq representor inner-product data
      {
        unsigned int Q_f_hat = n_F_terms*(n_F_terms+1)/2;

        auto fq_innerprods_list = rb_evaluation_reader.getFqInnerprods();
        if (fq_innerprods_list.size() != Q_f_hat)
          libmesh_error_msg("Size error while reading Fq representor norm data from buffer.");

        for (unsigned int i=0; i < Q_f_hat; ++i)
          rb_evaluation.Fq_representor_innerprods[i] = load_scalar_value(fq_innerprods_list[i]);
      }

      // Fq_Aq representor inner-product data
      {
        auto fq_aq_innerprods_list = rb_evaluation_reader.getFqAqInnerprods();
        if (fq_aq_innerprods_list.size() != n_F_terms*n_A_terms*n_bfs)
          libmesh_error_msg("Size error while reading Fq-Aq representor norm data from buffer.");

        for (unsigned int q_f=0; q_f<n_F_terms; ++q_f)
          for (unsigned int q_a=0; q_a<n_A_terms; ++q_a)
            for (unsigned int i=0; i<n_bfs; ++i)
              {
                unsigned int offset = q_f*n_A_terms*n_bfs + q_a*n_bfs + i;
                rb_evaluation.Fq_Aq_representor_innerprods[q_f][q_a][i] =
                  load_scalar_value(fq_aq_innerprods_list[offset]);
              }
      }

      // Aq_Aq representor inner-product data
      {
        unsigned int Q_a_hat = n_A_terms*(n_A_terms+1)/2;
        auto aq_aq_innerprods_list = rb_evaluation_reader.getAqAqInnerprods();
        if (aq_aq_innerprods_list.size() != Q_a_hat*n_bfs*n_bfs)
          libmesh_error_msg("Size error while reading Aq-Aq representor norm data from buffer.");

        for (unsigned int i=0; i<Q_a_hat; ++i)
          for (unsigned int j=0; j<n_bfs; ++j)
            for (unsigned int l=0; l<n_bfs; ++l)
              {
                unsigned int offset = i*n_bfs*n_bfs + j*n_bfs + l;
                rb_evaluation.Aq_Aq_representor_innerprods[i][j][l] =
                  load_scalar_value(aq_aq_innerprods_list[offset]);
              }
      }

      // Output dual inner-product data
      {
        unsigned int n_outputs = rb_theta_expansion.get_n_outputs();
        auto output_innerprod_outer = rb_evaluation_reader.getOutputDualInnerprods();

        if (output_innerprod_outer.size() != n_outputs)
          libmesh_error_msg("Incorrect number of outputs detected in the buffer");

        for (unsigned int output_id=0; output_id<n_outputs; ++output_id)
          {
            unsigned int n_output_terms = rb_theta_expansion.get_n_output_terms(output_id);

            unsigned int Q_l_hat = n_output_terms*(n_output_terms+1)/2;
            auto output_innerprod_inner = output_innerprod_outer[output_id];

            if (output_innerprod_inner.size() != Q_l_hat)
              libmesh_error_msg("Incorrect number of output terms detected in the buffer");

            for (unsigned int q=0; q<Q_l_hat; ++q)
              {
                rb_evaluation.output_dual_innerprods[output_id][q] =
                  load_scalar_value(output_innerprod_inner[q]);
              }
          }
      }
    }

  // Output vectors
  {
    unsigned int n_outputs = rb_theta_expansion.get_n_outputs();
    auto output_vector_outer = rb_evaluation_reader.getOutputVectors();

    if (output_vector_outer.size() != n_outputs)
      libmesh_error_msg("Incorrect number of outputs detected in the buffer");

    for (unsigned int output_id=0; output_id<n_outputs; ++output_id)
      {
        unsigned int n_output_terms = rb_theta_expansion.get_n_output_terms(output_id);

        auto output_vector_middle = output_vector_outer[output_id];
        if (output_vector_middle.size() != n_output_terms)
          libmesh_error_msg("Incorrect number of output terms detected in the buffer");

        for (unsigned int q_l=0; q_l<n_output_terms; ++q_l)
          {
            auto output_vectors_inner_list = output_vector_middle[q_l];

            if (output_vectors_inner_list.size() != n_bfs)
              libmesh_error_msg("Incorrect number of output terms detected in the buffer");

            for (unsigned int j=0; j<n_bfs; ++j)
              {
                rb_evaluation.RB_output_vectors[output_id][q_l](j) =
                  load_scalar_value(output_vectors_inner_list[j]);
              }
          }
      }
  }

  // Fq vectors and Aq matrices
  {
    auto rb_fq_vectors_outer_list = rb_evaluation_reader.getRbFqVectors();
    if (rb_fq_vectors_outer_list.size() != n_F_terms)
      libmesh_error_msg("Incorrect number of Fq vectors detected in the buffer");

    for (unsigned int q_f=0; q_f<n_F_terms; ++q_f)
      {
        auto rb_fq_vectors_inner_list = rb_fq_vectors_outer_list[q_f];
        if (rb_fq_vectors_inner_list.size() != n_bfs)
          libmesh_error_msg("Incorrect Fq vector size detected in the buffer");

        for (unsigned int i=0; i < n_bfs; ++i)
          {
            rb_evaluation.RB_Fq_vector[q_f](i) =
              load_scalar_value(rb_fq_vectors_inner_list[i]);
          }
      }

    auto rb_Aq_matrices_outer_list = rb_evaluation_reader.getRbAqMatrices();
    if (rb_Aq_matrices_outer_list.size() != n_A_terms)
      libmesh_error_msg("Incorrect number of Aq matrices detected in the buffer");

    for (unsigned int q_a=0; q_a<n_A_terms; ++q_a)
      {
        auto rb_Aq_matrices_inner_list = rb_Aq_matrices_outer_list[q_a];
        if (rb_Aq_matrices_inner_list.size() != n_bfs*n_bfs)
          libmesh_error_msg("Incorrect Aq matrix size detected in the buffer");

        for (unsigned int i=0; i<n_bfs; ++i)
          for (unsigned int j=0; j<n_bfs; ++j)
            {
              unsigned int offset = i*n_bfs+j;
              rb_evaluation.RB_Aq_vector[q_a](i,j) =
                load_scalar_value(rb_Aq_matrices_inner_list[offset]);
            }
      }
  }

  // Inner-product matrix
  if (rb_evaluation.compute_RB_inner_product)
    {
      auto rb_inner_product_matrix_list =
        rb_evaluation_reader.getRbInnerProductMatrix();

      if (rb_inner_product_matrix_list.size() != n_bfs*n_bfs)
        libmesh_error_msg("Size error while reading the inner product matrix.");

      for (unsigned int i=0; i<n_bfs; ++i)
        for (unsigned int j=0; j<n_bfs; ++j)
          {
            unsigned int offset = i*n_bfs + j;
            rb_evaluation.RB_inner_product_matrix(i,j) =
              load_scalar_value(rb_inner_product_matrix_list[offset]);
          }
    }
}

template <typename RBEvaluationReaderNumber, typename TransRBEvaluationReaderNumber>
void load_transient_rb_evaluation_data(TransientRBEvaluation & trans_rb_eval,
                                       RBEvaluationReaderNumber & rb_eval_reader,
                                       TransRBEvaluationReaderNumber & trans_rb_eval_reader,
                                       bool read_error_bound_data)
{
  load_rb_evaluation_data(trans_rb_eval,
                          rb_eval_reader,
                          read_error_bound_data);

  trans_rb_eval.set_delta_t( trans_rb_eval_reader.getDeltaT() );
  trans_rb_eval.set_euler_theta( trans_rb_eval_reader.getEulerTheta() );
  trans_rb_eval.set_n_time_steps( trans_rb_eval_reader.getNTimeSteps() );
  trans_rb_eval.set_time_step( trans_rb_eval_reader.getTimeStep() );

  unsigned int n_bfs = rb_eval_reader.getNBfs();
  unsigned int n_F_terms = trans_rb_eval.get_rb_theta_expansion().get_n_F_terms();
  unsigned int n_A_terms = trans_rb_eval.get_rb_theta_expansion().get_n_A_terms();

  TransientRBThetaExpansion & trans_theta_expansion =
    cast_ref<TransientRBThetaExpansion &>(trans_rb_eval.get_rb_theta_expansion());
  unsigned int n_M_terms = trans_theta_expansion.get_n_M_terms();

  // L2 matrix
  {
    auto rb_L2_matrix_list =
      trans_rb_eval_reader.getRbL2Matrix();

    if (rb_L2_matrix_list.size() != n_bfs*n_bfs)
      libmesh_error_msg("Size error while reading the L2 matrix.");

    for (unsigned int i=0; i<n_bfs; ++i)
      for (unsigned int j=0; j<n_bfs; ++j)
        {
          unsigned int offset = i*n_bfs + j;
          trans_rb_eval.RB_L2_matrix(i,j) =
            load_scalar_value(rb_L2_matrix_list[offset]);
        }
  }

  // Mq matrices
  {
    auto rb_Mq_matrices_outer_list = trans_rb_eval_reader.getRbMqMatrices();

    if (rb_Mq_matrices_outer_list.size() != n_M_terms)
      libmesh_error_msg("Incorrect number of Mq matrices detected in the buffer");

    for (unsigned int q_m=0; q_m < n_M_terms; ++q_m)
      {
        auto rb_Mq_matrices_inner_list = rb_Mq_matrices_outer_list[q_m];
        if (rb_Mq_matrices_inner_list.size() != n_bfs*n_bfs)
          libmesh_error_msg("Incorrect Mq matrix size detected in the buffer");

        for (unsigned int i=0; i<n_bfs; ++i)
          for (unsigned int j=0; j<n_bfs; ++j)
            {
              unsigned int offset = i*n_bfs+j;
              trans_rb_eval.RB_M_q_vector[q_m](i,j) =
                load_scalar_value(rb_Mq_matrices_inner_list[offset]);
            }
      }
  }

  // The initial condition and L2 error at t=0.
  {
    auto initial_l2_errors_reader =
      trans_rb_eval_reader.getInitialL2Errors();
    if (initial_l2_errors_reader.size() != n_bfs)
      libmesh_error_msg("Incorrect number of initial L2 error terms detected in the buffer");

    auto initial_conditions_outer_list =
      trans_rb_eval_reader.getInitialConditions();
    if (initial_conditions_outer_list.size() != n_bfs)
      libmesh_error_msg("Incorrect number of outer initial conditions detected in the buffer");

    for (unsigned int i=0; i<n_bfs; i++)
      {
        trans_rb_eval.initial_L2_error_all_N[i] =
          initial_l2_errors_reader[i];

        auto initial_conditions_inner_list = initial_conditions_outer_list[i];
        if (initial_conditions_inner_list.size() != (i+1))
          libmesh_error_msg("Incorrect number of inner initial conditions detected in the buffer");

        for (unsigned int j=0; j<=i; j++)
          {
            trans_rb_eval.RB_initial_condition_all_N[i](j) =
              load_scalar_value(initial_conditions_inner_list[j]);
          }
      }
  }


  if (read_error_bound_data)
    {
      // Fq_Mq data
      {
        auto fq_mq_innerprods_list = trans_rb_eval_reader.getFqMqInnerprods();
        if (fq_mq_innerprods_list.size() != n_F_terms*n_M_terms*n_bfs)
          libmesh_error_msg("Size error while reading Fq-Mq representor data from buffer.");

        for (unsigned int q_f=0; q_f<n_F_terms; ++q_f)
          for (unsigned int q_m=0; q_m<n_M_terms; ++q_m)
            for (unsigned int i=0; i<n_bfs; ++i)
              {
                unsigned int offset = q_f*n_M_terms*n_bfs + q_m*n_bfs + i;
                trans_rb_eval.Fq_Mq_representor_innerprods[q_f][q_m][i] =
                  load_scalar_value(fq_mq_innerprods_list[offset]);
              }
      }


      // Mq_Mq representor inner-product data
      {
        unsigned int Q_m_hat = n_M_terms*(n_M_terms+1)/2;
        auto mq_mq_innerprods_list = trans_rb_eval_reader.getMqMqInnerprods();
        if (mq_mq_innerprods_list.size() != Q_m_hat*n_bfs*n_bfs)
          libmesh_error_msg("Size error while reading Mq-Mq representor data from buffer.");

        for (unsigned int i=0; i<Q_m_hat; ++i)
          for (unsigned int j=0; j<n_bfs; ++j)
            for (unsigned int l=0; l<n_bfs; ++l)
              {
                unsigned int offset = i*n_bfs*n_bfs + j*n_bfs + l;
                trans_rb_eval.Mq_Mq_representor_innerprods[i][j][l] =
                  load_scalar_value(mq_mq_innerprods_list[offset]);
              }
      }

      // Aq_Mq representor inner-product data
      {
        auto aq_mq_innerprods_list =
          trans_rb_eval_reader.getAqMqInnerprods();
        if (aq_mq_innerprods_list.size() != n_A_terms*n_M_terms*n_bfs*n_bfs)
          libmesh_error_msg("Size error while reading Aq-Mq representor data from buffer.");

        for (unsigned int q_a=0; q_a<n_A_terms; q_a++)
          for (unsigned int q_m=0; q_m<n_M_terms; q_m++)
            for (unsigned int i=0; i<n_bfs; i++)
              for (unsigned int j=0; j<n_bfs; j++)
                {
                  unsigned int offset =
                    q_a*(n_M_terms*n_bfs*n_bfs) + q_m*(n_bfs*n_bfs) + i*n_bfs + j;

                  trans_rb_eval.Aq_Mq_representor_innerprods[q_a][q_m][i][j] =
                    load_scalar_value(aq_mq_innerprods_list[offset]);
                }
      }

    }
}

template <typename RBEvaluationReaderNumber, typename RBEIMEvaluationReaderNumber>
void load_rb_eim_evaluation_data(RBEIMEvaluation & rb_eim_evaluation,
                                 RBEvaluationReaderNumber & rb_evaluation_reader,
                                 RBEIMEvaluationReaderNumber & rb_eim_evaluation_reader)
{
  // We use read_error_bound_data=false here, since the RBEvaluation error bound data
  // is not relevant to EIM.
  load_rb_evaluation_data(rb_eim_evaluation,
                          rb_evaluation_reader,
                          /*read_error_bound_data*/ false);

  unsigned int n_bfs = rb_eim_evaluation.get_n_basis_functions();

  // EIM interpolation matrix
  {
    auto interpolation_matrix_list = rb_eim_evaluation_reader.getInterpolationMatrix();

    if (interpolation_matrix_list.size() != n_bfs*(n_bfs+1)/2)
      libmesh_error_msg("Size error while reading the eim inner product matrix.");

    for (unsigned int i=0; i<n_bfs; ++i)
      for (unsigned int j=0; j<=i; ++j)
        {
          unsigned int offset = i*(i+1)/2 + j;
          rb_eim_evaluation.interpolation_matrix(i,j) =
            load_scalar_value(interpolation_matrix_list[offset]);
        }
  }

  // Interpolation points
  {
    auto interpolation_points_list =
      rb_eim_evaluation_reader.getInterpolationPoints();

    if (interpolation_points_list.size() != n_bfs)
      libmesh_error_msg("Size error while reading the eim interpolation points.");

    rb_eim_evaluation.interpolation_points.resize(n_bfs);
    for (unsigned int i=0; i<n_bfs; ++i)
      {
        load_point(interpolation_points_list[i],
                   rb_eim_evaluation.interpolation_points[i]);
      }
  }

  // Interpolation points variables
  {
    auto interpolation_points_var_list =
      rb_eim_evaluation_reader.getInterpolationPointsVar();
    rb_eim_evaluation.interpolation_points_var.resize(n_bfs);

    if (interpolation_points_var_list.size() != n_bfs)
      libmesh_error_msg("Size error while reading the eim interpolation variables.");

    for (unsigned int i=0; i<n_bfs; ++i)
      {
        rb_eim_evaluation.interpolation_points_var[i] =
          interpolation_points_var_list[i];
      }
  }

  // Interpolation elements
  {
    libMesh::dof_id_type elem_id = 0;
    libMesh::ReplicatedMesh & interpolation_points_mesh =
      rb_eim_evaluation.get_interpolation_points_mesh();
    interpolation_points_mesh.clear();

    auto interpolation_points_elem_list =
      rb_eim_evaluation_reader.getInterpolationPointsElems();
    unsigned int n_interpolation_elems = interpolation_points_elem_list.size();

    if (n_interpolation_elems != n_bfs)
      libmesh_error_msg("The number of elements should match the number of basis functions");

    rb_eim_evaluation.interpolation_points_elem.resize(n_interpolation_elems);

    for (unsigned int i=0; i<n_interpolation_elems; ++i)
      {
        auto mesh_elem_reader = interpolation_points_elem_list[i];
        std::string elem_type_name = mesh_elem_reader.getType().cStr();
        libMesh::ElemType elem_type =
          libMesh::Utility::string_to_enum<libMesh::ElemType>(elem_type_name);

        libMesh::Elem * elem = libMesh::Elem::build(elem_type).release();
        elem->set_id(elem_id++);
        load_elem_into_mesh(mesh_elem_reader, elem, interpolation_points_mesh);

        rb_eim_evaluation.interpolation_points_elem[i] = elem;
      }
  }
}

#if defined(LIBMESH_HAVE_SLEPC) && (LIBMESH_HAVE_GLPK)
void load_rb_scm_evaluation_data(RBSCMEvaluation & rb_scm_eval,
                                 RBData::RBSCMEvaluation::Reader & rb_scm_evaluation_reader)
{
  auto parameter_ranges =
    rb_scm_evaluation_reader.getParameterRanges();
  auto discrete_parameters_list =
    rb_scm_evaluation_reader.getDiscreteParameters();
  load_parameter_ranges(rb_scm_eval,
                        parameter_ranges,
                        discrete_parameters_list);

  unsigned int n_A_terms = rb_scm_eval.get_rb_theta_expansion().get_n_A_terms();

  {
    auto b_min_list = rb_scm_evaluation_reader.getBMin();

    if (b_min_list.size() != n_A_terms)
      libmesh_error_msg("Size error while reading B_min");

    rb_scm_eval.B_min.clear();
    for (unsigned int i=0; i<n_A_terms; i++)
      rb_scm_eval.B_min.push_back(b_min_list[i]);
  }

  {
    auto b_max_list = rb_scm_evaluation_reader.getBMax();

    if (b_max_list.size() != n_A_terms)
      libmesh_error_msg("Size error while reading B_max");

    rb_scm_eval.B_max.clear();
    for (unsigned int i=0; i<n_A_terms; i++)
      rb_scm_eval.B_max.push_back(b_max_list[i]);
  }

  {
    auto cJ_stability_vector =
      rb_scm_evaluation_reader.getCJStabilityVector();

    rb_scm_eval.C_J_stability_vector.clear();
    for (const auto & val : cJ_stability_vector)
      rb_scm_eval.C_J_stability_vector.push_back(val);
  }

  {
    auto cJ_parameters_outer =
      rb_scm_evaluation_reader.getCJ();

    rb_scm_eval.C_J.resize(cJ_parameters_outer.size());
    for (auto i : index_range(cJ_parameters_outer))
      {
        auto cJ_parameters_inner =
          cJ_parameters_outer[i];

        for (auto j : index_range(cJ_parameters_inner))
          {
            std::string param_name = cJ_parameters_inner[j].getName();
            Real param_value = cJ_parameters_inner[j].getValue();
            rb_scm_eval.C_J[i].set_value(param_name, param_value);
          }
      }
  }

  {
    auto scm_ub_vectors =
      rb_scm_evaluation_reader.getScmUbVectors();

    // The number of UB vectors is the same as the number of C_J values
    unsigned int n_C_J_values = rb_scm_evaluation_reader.getCJ().size();

    if (scm_ub_vectors.size() != n_C_J_values*n_A_terms)
      libmesh_error_msg("Size mismatch in SCB UB vectors");

    rb_scm_eval.SCM_UB_vectors.resize( n_C_J_values );
    for (unsigned int i=0; i<n_C_J_values; i++)
      {
        rb_scm_eval.SCM_UB_vectors[i].resize(n_A_terms);
        for (unsigned int j=0; j<n_A_terms; j++)
          {
            unsigned int offset = i*n_A_terms + j;
            rb_scm_eval.SCM_UB_vectors[i][j] = scm_ub_vectors[offset];

          }
      }
  }

}
#endif // LIBMESH_HAVE_SLEPC && LIBMESH_HAVE_GLPK



void load_point(RBData::Point3D::Reader point_reader, Point & point)
{
  point(0) = point_reader.getX();

  if (LIBMESH_DIM >= 2)
    point(1) = point_reader.getY();

  if (LIBMESH_DIM >= 3)
    point(2) = point_reader.getZ();
}


void load_elem_into_mesh(RBData::MeshElem::Reader mesh_elem_reader,
                         libMesh::Elem * elem,
                         libMesh::ReplicatedMesh & mesh)
{
  auto mesh_elem_point_list = mesh_elem_reader.getPoints();
  unsigned int n_points = mesh_elem_point_list.size();

  if (n_points != elem->n_nodes())
    libmesh_error_msg("Wrong number of nodes for element type");

  for (unsigned int i=0; i < n_points; ++i)
    {
      libMesh::Node * node =
        mesh.add_point(Point(mesh_elem_point_list[i].getX(),
                             mesh_elem_point_list[i].getY(),
                             mesh_elem_point_list[i].getZ()));

      elem->set_node(i) = node;
    }

  elem->subdomain_id() = mesh_elem_reader.getSubdomainId();

  mesh.add_elem(elem);
}

// ---- Helper functions for adding data to capnp Builders (END) ----

} // namespace RBDataSerialization

} // namespace libMesh

#endif // #if defined(LIBMESH_HAVE_CAPNPROTO)
