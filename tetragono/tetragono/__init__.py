#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (C) 2020-2023 Hao Zhang<zh970205@mail.ustc.edu.cn>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

from mpi4py import MPI
import torch

if torch.cuda.device_count() != 0:
    torch.set_default_tensor_type(torch.cuda.FloatTensor)
    torch.cuda.set_device(
        MPI.COMM_WORLD.Split_type(MPI.COMM_TYPE_SHARED, MPI.COMM_WORLD.Get_rank()).Get_rank() %
        torch.cuda.device_count())

# States
from .abstract_state import AbstractState
from .exact_state import ExactState
from .abstract_lattice import AbstractLattice
from .simple_update_lattice import SimpleUpdateLattice
from .sampling_lattice import SamplingLattice, Configuration, Observer, SweepSampling, ErgodicSampling, DirectSampling

# Miscellaneous
from . import conversion
from . import common_tensor
from . import utility
from .utility import *
