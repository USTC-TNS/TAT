#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (C) 2022 Hao Zhang<zh970205@mail.ustc.edu.cn>
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

import torch
import TAT
import tetragono as tet


def ansatz(state, m):
    """
    The code here was designed by Xiao Liang.
    See https://link.aps.org/doi/10.1103/PhysRevB.98.104426 for more information.
    """
    max_int = 2**31
    random_int = TAT.random.uniform_int(0, max_int - 1)
    torch.manual_seed(random_int())
    network = torch.nn.Sequential(
        torch.nn.Conv2d(in_channels=1, out_channels=m, kernel_size=(3, 3), stride=(1, 1), padding=(1, 1)),
        torch.nn.MaxPool2d(kernel_size=(2, 2)),
        torch.nn.ConvTranspose2d(in_channels=m, out_channels=1, kernel_size=(2, 2), stride=(2, 2), padding=(0, 0)),
    ).double()
    return tet.multiple_product_state.ConvolutionalNeural(state, network)
