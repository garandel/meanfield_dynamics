from six import add_metaclass

from spinn_utilities.abstract_base import AbstractBase, abstractmethod, \
    abstractproperty
from spinn_utilities.ranged.ranged_list import RangedList
from spynnaker.pyNN.utilities.ranged.spynakker_ranged_list import \
    SpynakkerRangedList

from .abstract_settable import AbstractSettable


@add_metaclass(AbstractBase)
class AbstractPopulationSettable(AbstractSettable):
    """ Indicates that some properties of this object can be accessed from\
        the PyNN population set and get methods
    """

    __slots__ = ()

    @abstractproperty
    def n_atoms(self):
        """" See ApplicationVertex.n_atoms """
