"""
utility class containing simple helper methods
"""
from spynnaker.pyNN.models.neural_properties.randomDistributions \
    import RandomDistribution
from spinn_front_end_common.utilities import exceptions
import numpy
import os
import logging
import scipy.stats


logger = logging.getLogger(__name__)


def check_directory_exists_and_create_if_not(filename):
    """
    helper method for checking if a directory exists, and if not, create it
    :param filename:
    :return:
    """
    directory = os.path.dirname(filename)
    if directory != "" and not os.path.exists(directory):
        os.makedirs(directory)


def convert_param_to_numpy(param, no_atoms):
    """
    converts parameters into numpy arrays as needed
    :param param: the param to convert
    :param no_atoms: the number of atoms avilable for conversion of param
    :return the converted param in whatever format it was given
    """
    if RandomDistribution is None:
        raise exceptions.ConfigurationException(
            "Missing PyNN. Please install version 0.7.5 from "
            "http://neuralensemble.org/PyNN/")
    if isinstance(param, RandomDistribution):
        if no_atoms > 1:
            return numpy.asarray(param.next(n=no_atoms), dtype="float")
        else:
            return numpy.array([param.next(n=no_atoms)], dtype="float")
    elif not hasattr(param, '__iter__'):
        return numpy.array([param] * no_atoms, dtype="float")
    elif len(param) != no_atoms:
        raise exceptions.ConfigurationException("The number of params does"
                                                " not equal with the number"
                                                " of atoms in the vertex ")
    else:
        return numpy.array(param, dtype="float")


def write_parameters_per_neuron(spec, vertex_slice, parameters):
    for atom in range(vertex_slice.lo_atom, vertex_slice.hi_atom + 1):
        for param in parameters:
            value = param.get_value()
            if hasattr(value, "__len__"):
                if len(value) > 1:
                    value = value[atom]
                else:
                    value = value[0]

            spec.write_value(data=value,
                             data_type=param.get_dataspec_datatype())


def read_in_data_from_file(
        file_path, min_atom, max_atom, min_time, max_time):
    """method for helping code read in files of data values where the values are
    in a format of <Time><tab><atom_id><tab><data_value>

    :param file_path: absolute filepath to a file where gsyn values have been
    written
    :param min_atom: min neuron id to which neurons to read in
    :param max_atom: max neuron id to which neurons to read in
    :param min_time: min time slot to read neurons values of.
    :param max_time:max time slot to read neurons values of.
    :return: a numpi destacked array containing time stamps, neuron id and the
    data value.
    """
    times = list()
    atom_ids = list()
    data_items = list()

    with open(file_path, 'r') as fsource:
            read_data = fsource.readlines()

    for line in read_data:
        if not line.startswith('#'):
            values = line.split("\t")
            neuron_id = int(eval(values[1]))
            time = float(eval(values[0]))
            data_value = float(eval(values[2]))
            if (min_atom <= neuron_id < max_atom and
                    min_time <= time < max_time):
                times.append(time)
                atom_ids.append(neuron_id)
                data_items.append(data_value)
            else:
                print "failed to enter {}:{}".format(neuron_id, time)

    result = numpy.dstack((atom_ids, times, data_items))[0]
    result = result[numpy.lexsort((times, atom_ids))]
    return result


def read_spikes_from_file(file_path, min_atom, max_atom, min_time, max_time):
    """
    helper method for reading spikes from a file
    :param file_path: absolute filepath to a file where gsyn values have been
    written
    :param min_atom: min neuron id to which neurons to read in
    :param max_atom: max neuron id to which neurons to read in
    :param min_time: min time slot to read neurons values of.
    :param max_time:max time slot to read neurons values of.
    :return: a numpi destacked array containing time stamps, neuron id and the
    spike times.
    """
    spike_times = list()
    spike_ids = list()
    with open(file_path, 'r') as fsource:
            read_data = fsource.readlines()

    for line in read_data:
        if not line.startswith('#'):
            values = line.split("\t")
            neuron_id = int(eval(values[1]))
            time = float(eval(values[0]))
            if (min_atom <= neuron_id < max_atom and
                    min_time <= time < max_time):
                spike_times.append(time)
                spike_ids.append(neuron_id)

    result = numpy.dstack((spike_ids, spike_times))[0]
    result = result[numpy.lexsort((spike_times, spike_ids))]
    return result


# Converts between a distribution name, and the appropriate scipy stats for\
# that distribution
_distribution_to_scipy_stats = {
    'binomial': scipy.stats.binom,
    'gamma': scipy.stats.gamma,
    'exponential': scipy.stats.expon,
    'lognormal': scipy.stats.lognorm,
    'normal': scipy.stats.norm,
    'poisson': scipy.stats.poisson,
    'uniform': scipy.stats.uniform,
    'randint': scipy.stats.randint,
    'vonmises': scipy.stats.vonmises
}


def get_probability_within_range(dist, lower, upper):
    """ Get the probability that a value will fall within the given range for\
        a given RandomDistribution dist
    """
    scipy_stats = _distribution_to_scipy_stats[dist.name]
    return scipy_stats.cdf(lower, *dist.parameters)


def get_maximum_probable_value(dist, n_items, chance=(1.0 / 100.0)):
    """ Get the likely maximum value of a RandomDistribution given a\
        number of draws
    """
    scipy_stats = _distribution_to_scipy_stats[dist.name]
    prob = 1.0 - (chance / float(n_items))
    return scipy_stats.ppf(prob, *dist.parameters)


def get_minimum_probable_value(dist, n_items, chance=(1.0 / 100.0)):
    """ Get the likely minimum value of a RandomDistribution given a\
        number of draws
    """
    scipy_stats = _distribution_to_scipy_stats[dist.name]
    prob = chance / float(n_items)
    return scipy_stats.ppf(prob, *dist.parameters)


def get_mean(dist):
    """ Get the mean of a RandomDistribution
    """
    scipy_stats = _distribution_to_scipy_stats[dist.name]
    return scipy_stats.mean(*dist.parameters)


def get_standard_deviation(dist):
    """ Get the standard deviation of a RandomDistribution
    """
    scipy_stats = _distribution_to_scipy_stats[dist.name]
    return scipy_stats.std(*dist.parameters)


def get_variance(dist):
    """ Get the variance of a RandomDistribution
    """
    scipy_stats = _distribution_to_scipy_stats[dist.name]
    return scipy_stats.var(*dist.parameters)
