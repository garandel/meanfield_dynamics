/*
 * Copyright (c) 2017-2019 The University of Manchester
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*! \file
 * \brief implementation of the meanfield.h interface.
 * come from neuron.h
 * will change neuron for meanfield but for now need to see if it's work so
   no change just necessary
 * remove spikes and synapse because MF not need this.
 * Will implemente synapse like connection btw MFs after 1MF will work
 */

#include "../meanfield/meanfield.h"

#include "tdma_processing.h"
#include <debug.h>
#include "../meanfield/implementations/meanfield_impl.h"
#include "../meanfield/meanfield_recording.h"
#include "../meanfield/plasticity/synapse_dynamics.h"

//! The key to be used for this core (will be ORed with neuron ID)
key_t key;

//! A checker that says if this model should be transmitting. If set to false
//! by the data region, then this model should not have a key.
bool use_key;

//! Latest time in a timestep that any neuron has sent a spike
uint32_t latest_send_time = 0xFFFFFFFF;

//! Earliest time in a timestep that any neuron has sent a spike
uint32_t earliest_send_time = 0;

//! The number of neurons on the core
static uint32_t n_neurons;

//! The closest power of 2 >= n_neurons
static uint32_t n_neurons_peak;

//! The number of synapse types
static uint32_t n_synapse_types;

//! Amount to left shift the ring buffer by to make it an input
static uint32_t *ring_buffer_to_input_left_shifts;

//! The address where the actual neuron parameters start
static address_t saved_params_address;

//! parameters that reside in the neuron_parameter_data_region
struct neuron_parameters {
    uint32_t has_key;
    uint32_t transmission_key;
    uint32_t n_neurons_to_simulate;
    uint32_t n_neurons_peak;
    uint32_t n_synapse_types;
    uint32_t ring_buffer_shifts[];
};

//! \brief does the memory copy for the neuron parameters
//! \param[in] address: the address where the neuron parameters are stored
//!     in SDRAM
//! \return bool which is true if the mem copy's worked, false otherwise
static bool neuron_load_neuron_parameters(void) {
    log_debug("loading parameters");
    // call the neuron implementation functions to do the work
    // Note the "next" is 0 here because we are using a saved address
    // which has already accounted for the position of the data within
    // the region being read.
    neuron_impl_load_neuron_parameters(saved_params_address, 0, n_neurons);
    return true;
}

bool neuron_resume(void) { // EXPORTED
    if (!neuron_recording_reset(n_neurons)){
        log_error("failed to reload the neuron recording parameters");
        return false;
    }

    log_debug("neuron_reloading_neuron_parameters: starting");
    return neuron_load_neuron_parameters();
}

bool neuron_initialise(
        address_t address, address_t recording_address, // EXPORTED
        uint32_t *n_rec_regions_used) {
    log_debug("neuron_initialise: starting");
    
    log_info("&recording_address = 0x%08x",&recording_address);

    // init the TDMA
    void *data_addr = address;
    tdma_processing_initialise(&data_addr);

    // cast left over SDRAM into neuron struct.
    struct neuron_parameters *params = data_addr;

    // Check if there is a key to use
    use_key = params->has_key;

    // Read the spike key to use
    key = params->transmission_key;

    // output if this model is expecting to transmit
    if (!use_key) {
        log_info("\tThis model is not expecting to transmit as it has no key");
    } else {
        log_info("\tThis model is expected to transmit with key = %08x", key);
    }

    // Read the neuron details
    n_neurons = params->n_neurons_to_simulate;
    n_neurons_peak = params->n_neurons_peak;
    n_synapse_types = params->n_synapse_types;

    // Set up ring buffer left shifts
    uint32_t ring_buffer_bytes = n_synapse_types * sizeof(uint32_t);
    ring_buffer_to_input_left_shifts = spin1_malloc(ring_buffer_bytes);
    if (ring_buffer_to_input_left_shifts == NULL) {
        log_error("Not enough memory to allocate ring buffer");
        return false;
    }

    // read in ring buffer to input left shifts
    spin1_memcpy(
            ring_buffer_to_input_left_shifts, params->ring_buffer_shifts,
            ring_buffer_bytes);

    // Store where the actual neuron parameters start
    saved_params_address = &params->ring_buffer_shifts[n_synapse_types];

    log_info("\t n_neurons = %u, peak %u", n_neurons, n_neurons_peak);

    // Call the neuron implementation initialise function to setup DTCM etc.
    if (!meanfield_impl_initialise(n_neurons)) {
        return false;
    }

    // load the data into the allocated DTCM spaces.
    if (!neuron_load_neuron_parameters()) {
        return false;
    }

    // setup recording region
    if (!neuron_recording_initialise(
            recording_address, n_neurons, n_rec_regions_used)) {
        return false;
    }

    return true;
}

void neuron_pause(void) { // EXPORTED

    // call neuron implementation function to do the work
    neuron_impl_store_neuron_parameters(saved_params_address, 0, n_neurons);
}

void neuron_do_timestep_update(timer_t time, uint timer_count) { // EXPORTED

    // the phase in this timer tick im in (not tied to neuron index)
    // tdma_processing_reset_phase();

    // Prepare recording for the next timestep
    neuron_recording_setup_for_next_recording();

    neuron_impl_do_timestep_update(timer_count, time, n_neurons);

    log_debug("time left of the timer after tdma is %d", tc[T1_COUNT]);

    // Record the recorded variables
    neuron_recording_record(time);
}

//TO REMOVE AFTER
// do a converter but WILL change it for just a buffer with pointer once the problem
// will be solve
static union {
    uint32_t as_int;
    input_t as_real;
} number_bis;

/*
void neuron_transfer(weight_t *syns) { // EXPORTED
    uint32_t synapse_index = 0;
    uint32_t ring_buffer_index = 0;
    for (uint32_t s_i = n_synapse_types; s_i > 0; s_i--) {
        uint32_t rb_shift = ring_buffer_to_input_left_shifts[synapse_index];
        uint32_t neuron_index = 0;
        for (uint32_t n_i = n_neurons_peak; n_i > 0; n_i--) {
            weight_t value = syns[ring_buffer_index];
            //weight_t value2 = syns[ring_buffer_index];
            //log_info("value2 = %8.6k, ring_buffer_index = %8.6k, rb_shift=%5.8k", value2, ring_buffer_index, rb_shift);
            if (value > 0) {
                if (neuron_index > n_neurons) {
                    log_error("Neuron index %u out of range", neuron_index);
                    rt_error(RTE_SWERR);
                }
                input_t val_to_add = synapse_row_convert_weight_to_input(
                        value, rb_shift);
                
                number_bis.as_int = cc[CC_TXDATA];
                input_t val = number_bis.as_real;
                
                neuron_impl_add_inputs(synapse_index, neuron_index, val);//val=val_to_add normaly
                
                log_info("cc[CC_TXDATA] = %d", cc[CC_TXDATA]);
                log_info("neuron_index = %2.8k", neuron_index);
                //log_info("cc[CC_TXKEY] = %d", cc[CC_TXKEY]);
                 
                //neuron_impl_add_inputs(synapse_index, neuron_index, val);
                log_info("value = %8.6k, ring_buffer_index = %8.6k, val_to_add = %2.6k, rb_shift=%5.8k", value, ring_buffer_index, val_to_add, rb_shift);
            }
            syns[ring_buffer_index] = 0;
            ring_buffer_index++;
            neuron_index++;
        }
        synapse_index++;
    }
}
*/

//pourra utiliser la condition que je vien de supprimer pour test,
// afin de mettre une valeur de saturation 200MHz

void neuron_transfer(weight_t *syns) { // EXPORTED
    uint32_t synapse_index = 0;
    uint32_t ring_buffer_index = 0;
    for (uint32_t s_i = n_synapse_types; s_i > 0; s_i--) {
        uint32_t rb_shift = NULL;//ring_buffer_to_input_left_shifts[synapse_index];
        uint32_t neuron_index = 0;
        for (uint32_t n_i = n_neurons_peak; n_i > 0; n_i--) {
            weight_t value = syns[ring_buffer_index];
            if (neuron_index > n_neurons) {
                    log_error("Neuron index %u out of range", neuron_index);
                    rt_error(RTE_SWERR);
            }
            input_t val_to_add = synapse_row_convert_weight_to_input(value, rb_shift); ///modif ici pour avoir les input en direct
                            
            neuron_impl_add_inputs(synapse_index, neuron_index, val_to_add);//val=val_to_add normaly here do a artefact like /!\
                
            log_info("value = %d, ring_buffer_index = %d, val_to_add = %4.7k, rb_shift=%d", value, ring_buffer_index, val_to_add, rb_shift);
            
            syns[ring_buffer_index] = 0;
            ring_buffer_index++;
            neuron_index++;
        }
        synapse_index++;
    }
}
/*
void meanfield_transfer(input_t *syns) { // EXPORTED CONSTRUCTION
    uint32_t synapse_index = 0;
    uint32_t ring_buffer_index = 0;
    for (uint32_t s_i = n_synapse_types; s_i > 0; s_i--) {
        uint32_t rb_shift = 0.0;//ring_buffer_to_input_left_shifts[synapse_index];
        uint32_t neuron_index = 0;
        for (uint32_t n_i = n_neurons_peak; n_i > 0; n_i--) {
            weight_t value = syns[ring_buffer_index];
            if (neuron_index > n_neurons) {
                    log_error("Neuron index %u out of range", neuron_index);
                    rt_error(RTE_SWERR);
            }
            input_t val_to_add = synapse_row_convert_weight_to_input(value, rb_shift); ///modif ici pour avoir les input en direct
                            
            neuron_impl_add_inputs(synapse_index, neuron_index, val_to_add);//val=val_to_add normaly here do a artefact like /!\
                
            log_info("value = %4.7k, ring_buffer_index = %d, val_to_add = %4.7k, rb_shift=%d", value, ring_buffer_index, val_to_add, rb_shift);
            
            syns[ring_buffer_index] = 0;
            ring_buffer_index++;
            neuron_index++;
        }
        synapse_index++;
    }
}
*/

#if LOG_LEVEL >= LOG_DEBUG
void neuron_print_inputs(void) { // EXPORTED
    neuron_impl_print_inputs(n_neurons);
}

void neuron_print_synapse_parameters(void) { // EXPORTED
    neuron_impl_print_synapse_parameters(n_neurons);
}

const char *neuron_get_synapse_type_char(uint32_t synapse_type){//EXPORTED
    return neuron_impl_get_synapse_type_char(synapse_type);
}
#endif // LOG_LEVEL >= LOG_DEBUG
