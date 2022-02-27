#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (C) 2021-2022 Hao Zhang<zh970205@mail.ustc.edu.cn>
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

import TAT
import tetragono as tet


def create(L1, L2, D, Jx, Jy, Jz):
    """
    Create kitaev model lattice.
    see https://arxiv.org/pdf/cond-mat/0506438

    Parameters
    ----------
    L1, L2 : int
        The lattice size.
    D : int
        The cut dimension.
    Jx, Jy, Jz : float
        Rydberg atom parameters.
    """
    state = tet.AbstractState(TAT.No.Z.Tensor, L1, L2)
    for l1 in range(L1):
        for l2 in range(L2):
            if not (l1, l2) == (0, 0):
                state.physics_edges[l1, l2, 0] = 2
            if not (l1, l2) == (L1 - 1, L2 - 1):
                state.physics_edges[l1, l2, 1] = 2
    pauli_x_pauli_x = tet.common_variable.No.pauli_x_pauli_x.to(complex)
    pauli_y_pauli_y = tet.common_variable.No.pauli_y_pauli_y.to(complex)
    pauli_z_pauli_z = tet.common_variable.No.pauli_z_pauli_z.to(complex)
    Hx = -Jx * pauli_x_pauli_x
    Hy = -Jy * pauli_y_pauli_y
    Hz = -Jz * pauli_z_pauli_z
    for l1 in range(L1):
        for l2 in range(L2):
            if not ((l1, l2) == (0, 0) or (l1, l2) == (L1 - 1, L2 - 1)):
                state.hamiltonians[(l1, l2, 0), (l1, l2, 1)] = Hx
            if l1 != 0:
                state.hamiltonians[(l1 - 1, l2, 1), (l1, l2, 0)] = Hy
            if l2 != 0:
                state.hamiltonians[(l1, l2 - 1, 1), (l1, l2, 0)] = Hz
    state = tet.AbstractLattice(state)
    state.virtual_bond["R"] = D
    state.virtual_bond["D"] = D
    return state
