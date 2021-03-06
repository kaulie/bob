/**
 * @file bob/machine/LinearMachine.h
 * @date Tue May 31 09:22:33 2011 +0200
 * @author Andre Anjos <andre.anjos@idiap.ch>
 *
 * A machine that implements the liner projection of input to the output using
 * weights, biases and sums:
 * output = sum(inputs * weights) + bias
 * It is possible to setup the machine to previously normalize the input taking
 * into consideration some input bias and division factor. It is also possible
 * to set it up to have an activation function.
 * A linear classifier. See C. M. Bishop, "Pattern Recognition and Machine
 * Learning", chapter 4
 *
 * Copyright (C) 2011-2013 Idiap Research Institute, Martigny, Switzerland
 */

#ifndef BOB_MACHINE_LINEARMACHINE_H
#define BOB_MACHINE_LINEARMACHINE_H

#include <blitz/array.h>
#include <bob/io/HDF5File.h>
#include <bob/machine/Activation.h>

namespace bob { namespace machine {
  /**
   * @ingroup MACHINE
   * @{
   */

  /**
   * A linear classifier. See C. M. Bishop, "Pattern Recognition and Machine
   * Learning", chapter 4 for more details.
   */
  class LinearMachine {

    public: //api

      /**
       * Default constructor. Builds an otherwise invalid 0 x 0 linear machine.
       * This is equivalent to construct a LinearMachine with two size_t
       * parameters set to 0, as in LinearMachine(0, 0).
       */
      LinearMachine ();

      /**
       * Constructor, builds a new Linear machine. Weights and biases are
       * not initialized.
       *
       * @param input Size of input vector
       * @param output Size of output vector
       */
      LinearMachine (size_t input, size_t output);

      /**
       * Builds a new machine with a set of weights. Each column of the weight
       * matrix should represent a direction in which the input is projected
       * to.
       */
      LinearMachine(const blitz::Array<double,2>& weight);

      /**
       * Copies another machine
       */
      LinearMachine (const LinearMachine& other);

      /**
       * Starts a new LinearMachine from an existing Configuration object.
       */
      LinearMachine (bob::io::HDF5File& config);

      /**
       * Just to virtualise the destructor
       */
      virtual ~LinearMachine();

      /**
       * Assigns from a different machine
       */
      LinearMachine& operator= (const LinearMachine& other);

      /**
        * @brief Equal to
        */
      bool operator==(const LinearMachine& b) const;
      /**
        * @brief Not equal to
        */
      bool operator!=(const LinearMachine& b) const; 
      /**
       * @brief Similar to.
       */
      bool is_similar_to(const LinearMachine& b, const double r_epsilon=1e-5,
        const double a_epsilon=1e-8) const;

      /**
       * Loads data from an existing configuration object. Resets the current
       * state.
       */
      void load (bob::io::HDF5File& config);

      /**
       * Saves an existing machine to a Configuration object.
       */
      void save (bob::io::HDF5File& config) const;

      /**
       * Forwards data through the network, outputs the values of each linear
       * component the input signal is decomposed at.
       *
       * The input and output are NOT checked for compatibility each time. It
       * is your responsibility to do it.
       */
      void forward_ (const blitz::Array<double,1>& input,
          blitz::Array<double,1>& output) const;

      /**
       * Forwards data through the network, outputs the values of each linear
       * component the input signal is decomposed at.
       *
       * The input and output are checked for compatibility each time the
       * forward method is applied.
       */
      void forward (const blitz::Array<double,1>& input,
          blitz::Array<double,1>& output) const;

      /**
       * Resizes the machine. If either the input or output increases in size,
       * the weights and other factors should be considered uninitialized. If
       * the size is preserved or reduced, already initialized values will not
       * be changed. 
       *
       * Tip: Use this method to force data compression. All will work out
       * given most relevant factors to be preserved are organized on the top
       * of the weight matrix. In this way, reducing the system size will
       * supress less relevant projections.
       */
      void resize(size_t n_input, size_t n_output);

      /**
       * Returns the number of inputs expected by this machine
       */
      inline size_t inputSize () const { return m_weight.extent(0); }

      /**
       * Returns the number of outputs generated by this machine
       */
      inline size_t outputSize () const { return m_weight.extent(1); }

      /**
       * Returns the input subtraction factor
       */
      inline const blitz::Array<double, 1>& getInputSubtraction() const
      { return m_input_sub; }

      /**
       * Sets the current input subtraction factor. We will check that the
       * number of inputs (first dimension of weights) matches the number of
       * values currently set and will raise an exception if that is not the
       * case.
       */
      void setInputSubtraction(const blitz::Array<double,1>& v);

      /**
       * Returns the current input subtraction factor in order to be updated.
       * @warning Use with care. Only trainers should use this function for
       * efficiency reasons.
       */
      inline blitz::Array<double, 1>& updateInputSubtraction()  
      { return m_input_sub; }

      /**
       * Sets all input subtraction values to a specific value.
       */
      inline void setInputSubtraction(double v) { m_input_sub = v; }

      /**
       * Returns the input division factor
       */
      inline const blitz::Array<double, 1>& getInputDivision() const
      { return m_input_div; }

      /**
       * Sets the current input division factor. We will check that the number
       * of inputs (first dimension of weights) matches the number of values
       * currently set and will raise an exception if that is not the case.
       */
      void setInputDivision(const blitz::Array<double,1>& v);

      /**
       * Returns the current input division factor in order to be updated.
       * @warning Use with care. Only trainers should use this function for
       * efficiency reasons.
       */
      inline blitz::Array<double, 1>& updateInputDivision()  
      { return m_input_div; }


      /**
       * Sets all input division values to a specific value.
       */
      inline void setInputDivision(double v) { m_input_div = v; }

      /**
       * Returns the current weight representation. Each column should be
       * considered as a vector from which each of the output values is derived
       * by projecting the input onto such a vector.
       */
      inline const blitz::Array<double, 2>& getWeights() const 
      { return m_weight; }

      /**
       * Sets the current weights. We will check that the number of outputs and
       * inputs matches the expected values inside this machine and will raise
       * an exception if that is not the case.
       */
      void setWeights(const blitz::Array<double,2>& weight);

      /**
       * Returns the current weight representation in order to be updated. 
       * Each column should be considered as a vector from which each of the
       * output values is derived by projecting the input onto such a vector.
       * @warning Use with care. Only trainers should use this function for
       * efficiency reasons.
       */
      inline blitz::Array<double, 2>& updateWeights()  
      { return m_weight; }

      /**
       * Sets all weights to a single specific value.
       */
      inline void setWeights(double v) { m_weight = v; }

      /**
       * Returns the biases of this classifier.
       */
      inline const blitz::Array<double, 1>& getBiases() const 
      { return m_bias; }

      /**
       * Sets the current biases. We will check that the number of biases
       * matches the number of weights (first dimension) currently set and
       * will raise an exception if that is not the case.
       */
      void setBiases(const blitz::Array<double,1>& bias);

      /**
       * Sets all output bias values to a specific value.
       */
      inline void setBiases(double v) { m_bias = v; }

      /**
       * Returns the currently set activation function
       */
      inline boost::shared_ptr<Activation> getActivation() const { return m_activation; }

      /**
       * Sets the activation function for each of the outputs.
       */
      void setActivation(boost::shared_ptr<Activation> a);

    private: //representation

      typedef double (*actfun_t)(double); ///< activation function type

      blitz::Array<double, 1> m_input_sub; ///< input subtraction
      blitz::Array<double, 1> m_input_div; ///< input division
      blitz::Array<double, 2> m_weight; ///< weights
      blitz::Array<double, 1> m_bias; ///< biases for the output
      boost::shared_ptr<Activation> m_activation; ///< currently set activation type

      mutable blitz::Array<double, 1> m_buffer; ///< a buffer for speed
  
  };

  /**
   * @}
   */
}}

#endif /* BOB_MACHINE_LINEARMACHINE_H */
