# spynnaker imports
from spynnaker.pyNN.models.neural_projections.projection_partitionable_edge \
    import ProjectionPartitionableEdge
from spynnaker.pyNN.utilities import conf

# spinn front end common imports
from spinn_front_end_common.utilities.timer import Timer

# pacman imports
from pacman.utilities.progress_bar import ProgressBar

# general imports
import math
import logging
import copy


logger = logging.getLogger(__name__)


class DelayPartitionableEdge(ProjectionPartitionableEdge):
    """ An edge between a DelayExtensionVertex and an AbstractPopulationVertex
    """

    def __init__(self, delay_vertex, post_vertex, synapse_information,
                 label=None):
        ProjectionPartitionableEdge.__init__(
            self, delay_vertex, post_vertex, synapse_information,
            label=label)
        self._stored_synaptic_data_from_machine = None

    def get_n_synapse_rows(self, pre_vertex_slice=None):
        if pre_vertex_slice is not None:
            return pre_vertex_slice.n_atoms * self._pre_vertex.max_stages
        return self._pre_vertex.n_atoms * self._pre_vertex.max_stages

    def _get_delay_stage_max_n_words(
            self, pre_vertex_slice, post_vertex_slice, stage):
        min_delay = ((stage + 1) * self.pre_vertex.max_delay_per_neuron) + 1
        max_delay = min_delay + self.pre_vertex.max_delay_per_neuron
        for synapse_info in self.synapse_information:
            synapse_dynamics = synapse_info.synapse_dynamics
            connector = synapse_info.connector
            return synapse_dynamics.get_delayed_synapses_sdram_usage_in_bytes(
                connector, pre_vertex_slice, post_vertex_slice,
                min_delay, max_delay)

    def get_synapses_size_in_bytes(self, pre_vertex_slice, post_vertex_slice):
        return max([self._get_delay_stage_max_n_words(
                    pre_vertex_slice, post_vertex_slice, stage)
                    for stage in range(self._pre_vertex.max_stages)])

    def get_synaptic_list_from_machine(self, graph_mapper, partitioned_graph,
                                       placements, transceiver, routing_infos):
        """
        Get synaptic data for all connections in this Projection from the
        machine.
        :param graph_mapper:
        :param partitioned_graph:
        :param placements:
        :param transceiver:
        :param routing_infos:
        :return:
        """
        if self._stored_synaptic_data_from_machine is None:
            timer = None
            if conf.config.getboolean("Reports", "outputTimesForSections"):
                timer = Timer()
                timer.start_timing()

            subedges = \
                graph_mapper.get_partitioned_edges_from_partitionable_edge(
                    self)
            if subedges is None:
                subedges = list()

            synaptic_list = copy.copy(self._synapse_list)
            synaptic_list_rows = synaptic_list.get_rows()
            progress_bar = ProgressBar(
                len(subedges),
                "Reading back synaptic matrix for delayed edge between"
                " {} and {}".format(self._pre_vertex.label,
                                    self._post_vertex.label))
            for subedge in subedges:
                n_rows = subedge.get_n_rows(graph_mapper)
                pre_vertex_slice = \
                    graph_mapper.get_subvertex_slice(subedge.pre_subvertex)
                post_vertex_slice = \
                    graph_mapper.get_subvertex_slice(subedge.post_subvertex)

                sub_edge_post_vertex = \
                    graph_mapper.get_vertex_from_subvertex(
                        subedge.post_subvertex)
                rows = sub_edge_post_vertex.get_synaptic_list_from_machine(
                    placements, transceiver, subedge.pre_subvertex, n_rows,
                    subedge.post_subvertex,
                    self._synapse_row_io, partitioned_graph,
                    routing_infos, subedge.weight_scales).get_rows()

                for i in range(len(rows)):
                    delay_stage = math.floor(
                        float(i) / float(pre_vertex_slice.n_atoms)) + 1
                    min_delay = (delay_stage *
                                 self.pre_vertex.max_delay_per_neuron)
                    max_delay = (min_delay +
                                 self.pre_vertex.max_delay_per_neuron - 1)
                    synaptic_list_rows[
                        (i % pre_vertex_slice.n_atoms) +
                        pre_vertex_slice.lo_atom].set_slice_values(
                            rows[i], post_vertex_slice, min_delay, max_delay)
                progress_bar.update()
            progress_bar.end()
            self._stored_synaptic_data_from_machine = synaptic_list

            if conf.config.getboolean("Reports", "outputTimesForSections"):
                logger.info("Time to read matrix: {}".format(
                    timer.take_sample()))

        return self._stored_synaptic_data_from_machine
